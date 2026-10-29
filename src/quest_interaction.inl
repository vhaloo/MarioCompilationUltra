// Members of QuestPresentation::Impl. OpenXR/SDL lifetime belongs to the presenter.
bool handsSupported = false, handAimSupported = false, deckPlaced = false, nativePlaced = false;
PFN_xrCreateHandTrackerEXT createHandTracker = nullptr;
PFN_xrLocateHandJointsEXT locateHandJoints = nullptr;
PFN_xrDestroyHandTrackerEXT destroyHandTracker = nullptr;
std::array<XrHandTrackerEXT, 2> handTrackers{};
struct SpatialPointer {
    XrPosef ray = identity(), gripPose = identity();
    XrVector3f tip{};
    bool valid = false, gripValid = false, hand = false, tipValid = false, press = false,
         grasp = false;
};
std::array<SpatialPointer, 2> spatialPointers{};
std::array<bool, 2> wasSpatialPress{}, wasGrasp{}, uiConsumed{};
std::array<int, 2> capturedButton{{-1, -1}};
XrSpatialControls spatial;
std::vector<SpatialButton> deckButtons = spatialButtons(0);
SDL_Texture *deckLabels = nullptr;
uint64_t deckLabelHash = 0, controllerMissingSince = 0;
int deckPage = 0, moveTarget = 0, singleGrabHand = -1, singleGrabTarget = -1;
bool autoJoystick = false, spatialRearm = true;
XrPosef singleFromGrip = identity(), singleFromObject = identity();

bool actionPoseActive(int hand) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = aim;
    get.subactionPath = hands[hand];
    XrActionStatePose value{XR_TYPE_ACTION_STATE_POSE};
    return XR_SUCCEEDED(xrGetActionStatePose(session, &get, &value)) && value.isActive;
}
void initialiseHands() {
    if (!handsSupported || !handAimSupported)
        return;
    XrSystemHandTrackingPropertiesEXT h{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
    XrSystemProperties p{XR_TYPE_SYSTEM_PROPERTIES};
    p.next = &h;
    if (XR_FAILED(xrGetSystemProperties(instance, system, &p)) || !h.supportsHandTracking)
        return;
    auto proc = [&](const char *n, auto &out) {
        return XR_SUCCEEDED(xrGetInstanceProcAddr(instance, n,
                                                  reinterpret_cast<PFN_xrVoidFunction *>(&out))) &&
               out;
    };
    if (!proc("xrCreateHandTrackerEXT", createHandTracker) ||
        !proc("xrLocateHandJointsEXT", locateHandJoints) ||
        !proc("xrDestroyHandTrackerEXT", destroyHandTracker))
        return;
    for (int i = 0; i < 2; ++i) {
        XrHandTrackerCreateInfoEXT info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
        info.hand = i == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
        info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
        if (XR_FAILED(createHandTracker(session, &info, &handTrackers[i])))
            handTrackers[i] = XR_NULL_HANDLE;
    }
}
SpatialPointer readPointer(int hand, float triggerValue, float gripValue) {
    SpatialPointer p;
    auto a = locate(aimSpaces[hand]), g = locate(gripSpaces[hand]);
    if (actionPoseActive(hand) && valid(a)) {
        p.valid = true;
        p.ray = a.pose;
        p.gripValid = valid(g);
        p.gripPose = g.pose;
        p.press = spatialPress(triggerValue, wasSpatialPress[hand], true);
        p.grasp = spatialPress(gripValue, wasGrasp[hand], p.gripValid);
        return p;
    }
    if (!app.settings.questHandTracking || !handTrackers[hand] || !locateHandJoints)
        return p;
    std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints{};
    XrHandTrackingAimStateFB aimState{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
    XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
    locations.next = &aimState;
    locations.jointCount = uint32_t(joints.size());
    locations.jointLocations = joints.data();
    XrHandJointsLocateInfoEXT info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
    info.baseSpace = local;
    info.time = frame.predictedDisplayTime;
    if (XR_FAILED(locateHandJoints(handTrackers[hand], &info, &locations)) || !locations.isActive)
        return p;
    if (!(aimState.status & XR_HAND_TRACKING_AIM_VALID_BIT_FB) ||
        (aimState.status & XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB))
        return p;
    p.hand = p.valid = true;
    p.ray = aimState.aimPose;
    auto palm = joints[XR_HAND_JOINT_PALM_EXT], tip = joints[XR_HAND_JOINT_INDEX_TIP_EXT];
    constexpr auto validPose =
        XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    p.gripValid = (palm.locationFlags & validPose) == validPose;
    p.gripPose = palm.pose;
    p.tipValid = (tip.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
    p.tip = tip.pose.position;
    p.press = spatialPress(aimState.pinchStrengthIndex, wasSpatialPress[hand], true);
    p.grasp = spatialPress(aimState.pinchStrengthMiddle, wasGrasp[hand], p.gripValid);
    return p;
}
void placeDeck(bool force = false) {
    if (!haveHead)
        return;
    auto yaw = yawOrientation();
    if (!deckPlaced || force) {
        spatial.pose = {yaw, add(head.position, rotate(yaw, {.73f, -.38f, -1.15f}))};
        deckPlaced = true;
    }
    if (!nativePlaced || force) {
        spatial.nativePose = {yaw, add(head.position, rotate(yaw, {-.70f, -.18f, -1.3f}))};
        nativePlaced = true;
    }
}
struct SpatialHit {
    bool hit = false;
    float x = 0, y = 0, z = 0, distance = 0;
};
SpatialHit hitPlane(const SpatialPointer &p, XrPosef pose, float scale, float z = 0) {
    if (!p.valid || scale <= 0)
        return {};
    auto q = inverse(pose.orientation);
    auto o = rotate(q, sub(p.ray.position, pose.position));
    auto d = rotate(q, rotate(p.ray.orientation, {0, 0, -1}));
    if (d.z >= -.001f)
        return {};
    float t = (z * scale - o.z) / d.z;
    if (t <= 0 || t > 30)
        return {};
    auto h = mul(add(o, mul(d, t)), 1 / scale);
    return {true, h.x, h.y, h.z, t};
}
void changeDeckPage(int page) {
    deckPage = std::clamp(page, 0, 6);
    deckButtons = spatialButtons(deckPage);
    capturedButton = {{-1, -1}};
    spatial.hover = spatial.pressed = {{-1, -1}};
    deckLabelHash = 0;
}
void spatialAction(int action, float dt) {
    auto &s = app.settings;
    if (action >= PageModes && action <= PageNudge) {
        changeDeckPage(action - PageModes);
        return;
    }
    if (action >= ModeFirst && action < ModeFirst + 7) {
        s.questMode = action - ModeFirst;

        if (s.questMode > 0 && s.questMode < 4)
            s.questPassthrough = true;
        needRecenter = true;
        if (app.core.loaded && app.menu)
            app.toggleMenu();
        app.dirty = true;
        return;
    }
    if (action >= SectionFirst && action < SectionFirst + 7) {
        app.showSection(action - SectionFirst);
        placeMenu();
        return;
    }
    auto shift = [&](XrVector3f delta) {
        auto d = rotate(yawOrientation(), delta);
        if (moveTarget == 1)
            spatial.nativePose.position = add(spatial.nativePose.position, d);
        else if (moveTarget == 2)
            spatial.pose.position = add(spatial.pose.position, d);
        else if (moveTarget == 3)
            menuPanel.position = add(menuPanel.position, d);
        else if (!s.questWallLocked) {
            if (fixed()) {
                wallBase.position = add(wallBase.position, d);
                selectedWall = useSavedAnchor = false;
                automaticPlaced = true;
            } else
                panel.position = add(panel.position, d);
        }
    };
    auto turn = [&](float yaw, float pitch) {
        XrPosef *p = moveTarget == 1   ? &spatial.nativePose
                     : moveTarget == 2 ? &spatial.pose
                     : moveTarget == 3 ? &menuPanel
                     : fixed()         ? &wallBase
                                       : &panel;
        if (moveTarget == 0 && s.questWallLocked)
            return;
        XrQuaternionf yq{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)},
            xq{std::sin(pitch / 2), 0, 0, std::cos(pitch / 2)};
        p->orientation = product(yq, product(p->orientation, xq));
        if (moveTarget == 0) {
            selectedWall = useSavedAnchor = false;
            automaticPlaced = true;
        }
    };
    switch (action) {
    case MoveGame:
        moveTarget = 0;
        s.questWallLocked = false;
        break;
    case MoveNative:
        moveTarget = mode() == 5 ? 0 : 1;
        if (mode() == 5)
            s.questWallLocked = false;
        s.questInsetCrt = true;
        break;
    case MoveRemote:
        moveTarget = 2;
        break;
    case MoveOptions:
        moveTarget = 3;
        app.showSection(6);
        placeMenu();
        break;
    case LockPlacement:
        s.questWallLocked = !s.questWallLocked;
        break;
    case Recover:
        s.questRemote = true;
        placeMenu();
        placeDeck(true);
        if (moveTarget == 0)
            needRecenter = true;
        break;
    case Smaller:
    case Larger:
    case ResetSize: {
        int *v = moveTarget == 1   ? &s.questInsetWidth
                 : moveTarget == 2 ? &s.questRemoteScale
                 : moveTarget == 3 ? &s.questOptionsScale
                                   : &s.questScale;
        int lo = moveTarget == 0 ? 5 : moveTarget == 1 ? 25 : moveTarget == 2 ? 50 : 50;
        int hi = moveTarget == 0 ? 20000 : moveTarget == 1 ? 160 : moveTarget == 2 ? 180 : 180;
        *v = std::clamp(action == ResetSize ? (moveTarget == 1 ? 60 : 100)
                        : action == Smaller ? *v / 2
                                            : *v * 2,
                        lo, hi);
        break;
    }
    case Cinema:
    case Tabletop:
    case HeadFollow:
        s.questMode = 0;
        s.questAnchor = action == HeadFollow ? 1 : 0;
        s.questTilt = action == Tabletop ? 55 : 0;
        s.questHeight = action == Tabletop ? -40 : 0;
        s.questDistance = action == HeadFollow ? 100 : 220;
        s.questWidth = action == HeadFollow ? 160 : 320;
        needRecenter = true;
        if (app.core.loaded && app.menu)
            app.toggleMenu();
        break;
    case MoveLeft:
        shift({-dt * .45f, 0, 0});
        break;
    case MoveRight:
        shift({dt * .45f, 0, 0});
        break;
    case MoveUp:
        shift({0, dt * .45f, 0});
        break;
    case MoveDown:
        shift({0, -dt * .45f, 0});
        break;
    case YawLeft:
        turn(-dt * .65f, 0);
        break;
    case YawRight:
        turn(dt * .65f, 0);
        break;
    case PitchUp:
        turn(0, dt * .65f);
        break;
    case PitchDown:
        turn(0, -dt * .65f);
        break;
    case MoveNear:
        shift({0, 0, dt * .65f});
        break;
    case MoveFar:
        shift({0, 0, -dt * .65f});
        break;
    case ToggleAr:
        s.questPassthrough = !s.questPassthrough;
        break;
    case ToggleTransparent:
        s.questTransparent = !s.questTransparent;
        break;
    case ToggleNative:
        s.questInsetCrt = !s.questInsetCrt;
        break;
    case ToggleGhosts:
        s.enemyPreview = !s.enemyPreview;
        break;
    case ToggleObjects:
        s.objectPreview = !s.objectPreview;
        break;
    case ToggleBackdrop:
        s.questWorldBackdrop = !s.questWorldBackdrop;
        break;
    case ToggleHints:
        s.questControlHints = !s.questControlHints;
        break;
    case CycleCrt:
        s.questCrt = (s.questCrt + 1) % 4;
        break;
    case CycleTvModel:
        s.questTvModel = (s.questTvModel + 1) % 3;
        break;
    case ToggleCoplanar:
        s.questCrtCoplanar = !s.questCrtCoplanar;
        break;
    case Refresh72:
        s.questRefresh = 72;
        break;
    case Refresh90:
        s.questRefresh = 90;
        break;
    case Refresh120:
        s.questRefresh = 120;
        break;
    case DetailLow:
    case DetailBalanced:
    case DetailHigh:
        s.questWallDetail = action - DetailLow;
        break;
    case EyeLow:
    case EyeBalanced:
    case EyeHigh:
        s.questEyeDetail = action - EyeLow;
        break;
    case ScrollUp:
        wheel = dt * 10;
        break;
    case ScrollDown:
        wheel = -dt * 10;
        break;
    case Advanced:
        app.showSection(6);
        placeMenu();
        break;
    case Library:
        app.showSection(0);
        placeMenu();
        break;
    case Resume:
        if (app.core.loaded)
            app.toggleMenu();
        else {
            app.showSection(0);
            placeMenu();
        }
        break;
    }
    app.dirty = true;
}
void updateSpatial(float lt, float rt, float &lg, float &rg) {
    auto &s = app.settings;
    placeDeck();
    spatial.visible = s.questRemote;
    spatial.buttons = &deckButtons;
    spatial.stick = {};
    spatial.nativeVisible =
        s.questInsetCrt && app.core.loaded && mode() != 5 && (fixed() || s.view != 0);
    spatial.hover = spatial.pressed = {{-1, -1}};
    spatial.rayLength = {};
    spatial.fingerVisible = {};
    spatial.active.clear();
    for (auto b : deckButtons)
        spatial.active.push_back(spatialActive(b.action));
    spatialPointers[0] = readPointer(0, lt, lg);
    spatialPointers[1] = readPointer(1, rt, rg);
    if (spatialRearm) {
        bool tracked = spatialPointers[0].valid || spatialPointers[1].valid;
        bool released = !spatialPointers[0].press && !spatialPointers[1].press &&
                        !spatialPointers[0].grasp && !spatialPointers[1].grasp;
        if (tracked && released)
            spatialRearm = false;
        for (auto &p : spatialPointers)
            p.press = p.grasp = false;
    }
    bool controller = (!spatialPointers[0].hand && spatialPointers[0].valid) ||
                      (!spatialPointers[1].hand && spatialPointers[1].valid);
    for (auto pad : app.pads)
        if (pad && SDL_GameControllerGetAttached(pad) &&
            SDL_GameControllerGetJoystick(pad) != joystick)
            controller = true;
    uint64_t now = SDL_GetTicks64();
    if (controller)
        controllerMissingSince = 0;
    else if (!controllerMissingSince)
        controllerMissingSince = now;
    bool wantJoystick =
        s.questHandJoystick == 1 || (s.questHandJoystick == 0 && !controller &&
                                     controllerMissingSince && now - controllerMissingSince > 750);
    if (wantJoystick && !autoJoystick) {
        changeDeckPage(2);
        spatial.visible = true;
    }
    autoJoystick = wantJoystick;
    spatial.visible |= wantJoystick;
    float dt = std::clamp(float(frame.predictedDisplayPeriod) * 1e-9f, 0.f, .05f);
    int pending = -1;
    for (int hand = 0; hand < 2; ++hand) {
        auto &p = spatialPointers[hand];
        uiConsumed[hand] = false;
        if (p.hand) {
            if (hand == 0)
                lg = p.grasp ? 1.f : 0.f;
            else
                rg = p.grasp ? 1.f : 0.f;
        }
        auto h = hitPlane(p, spatial.pose, s.questRemoteScale * .01f, .05f);
        bool poking = false;
        if (p.tipValid && spatial.visible) {
            auto tip =
                mul(rotate(inverse(spatial.pose.orientation), sub(p.tip, spatial.pose.position)),
                    100.f / s.questRemoteScale);
            if (tip.z > -.03f && tip.z < .10f && std::abs(tip.x) < spatialWidth / 2 &&
                std::abs(tip.y) < spatialHeight / 2) {
                h = {true, tip.x, tip.y, tip.z, 0};
                poking = tip.z < .045f;
            }
        }
        spatial.rays[hand] = p.ray;
        spatial.fingerTips[hand] = p.tip;
        spatial.fingerVisible[hand] = p.hand && p.valid && p.tipValid;
        spatial.rayLength[hand] = p.valid && (spatial.visible || app.menu)
                                      ? (h.hit ? std::min(3.f, h.distance) : 2.f)
                                      : 0;
        int hover = spatial.visible && h.hit ? spatialHit(deckButtons, h.x, h.y) : -1;
        bool press = p.valid && (p.press || poking);
        if (!p.valid || !press)
            capturedButton[hand] = -1;
        if (press && !wasSpatialPress[hand] && hover >= 0)
            capturedButton[hand] = hover;
        int at = capturedButton[hand];
        if (at >= int(deckButtons.size()))
            at = capturedButton[hand] = -1;
        spatial.hover[hand] = hover;
        spatial.pressed[hand] = press ? at : -1;
        uiConsumed[hand] = hover >= 0 || (press && at >= 0);
        if (press && at >= 0) {
            auto b = deckButtons[at];
            if (b.action == GameStick) {
                auto axis = h.hit ? spatialJoystick(h.x - b.x, h.y - b.y) : SpatialAxis{};
                spatial.stick = axis;
                if (!app.menu) {
                    if (axis.x < -.28f)
                        app.spatialButtons |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT;
                    if (axis.x > .28f)
                        app.spatialButtons |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT;
                    if (axis.y < -.28f)
                        app.spatialButtons |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN;
                    if (axis.y > .28f)
                        app.spatialButtons |= 1 << RETRO_DEVICE_ID_JOYPAD_UP;
                }
            } else if (!app.menu && b.action >= GameA && b.action <= GameX) {
                if (b.action == GameRewind)
                    app.spatialRewind = true;
                else {
                    int bit = b.action == GameA        ? RETRO_DEVICE_ID_JOYPAD_Y
                              : b.action == GameB      ? RETRO_DEVICE_ID_JOYPAD_B
                              : b.action == GameC      ? RETRO_DEVICE_ID_JOYPAD_A
                              : b.action == GameX      ? RETRO_DEVICE_ID_JOYPAD_X
                              : b.action == GameL      ? RETRO_DEVICE_ID_JOYPAD_L
                              : b.action == GameR      ? RETRO_DEVICE_ID_JOYPAD_R
                              : b.action == GameSelect ? RETRO_DEVICE_ID_JOYPAD_SELECT
                                                       : RETRO_DEVICE_ID_JOYPAD_START;
                    if (b.action == GameBoth)
                        app.spatialButtons |=
                            (1 << RETRO_DEVICE_ID_JOYPAD_Y) | (1 << RETRO_DEVICE_ID_JOYPAD_B);
                    else
                        app.spatialButtons |= 1 << bit;
                }
            } else if ((!wasSpatialPress[hand] || b.hold) && b.action != GameStick &&
                       b.action < GameA + 0)
                pending = b.action;
            else if (!wasSpatialPress[hand] && (b.action == Resume || b.action >= ModeFirst))
                pending = b.action;
            if (!wasSpatialPress[hand] && !p.hand)
                feedback();
        }
        // Gripping the remote's top handle overrides the currently selected target.
        bool handle = h.hit && std::abs(h.x) < .42f && h.y > .31f && h.y < .40f && spatial.visible;
        auto nh = hitPlane(p, spatial.nativePose, s.questInsetWidth * .01f, .1f);
        bool nativeHandle = spatial.nativeVisible && nh.hit && std::abs(nh.x) < .34f &&
                            nh.y > -.54f && nh.y < -.40f;
        auto mh = hitPlane(p, menuPanel, 1);
        float mw = s.questMenuDistance * .014f * s.questOptionsScale * .01f, mhgt = mw * 9 / 16;
        bool menuHandle = app.menu && menuPlaced && mh.hit && std::abs(mh.x) < mw / 2 &&
                          mh.y > mhgt * .40f && mh.y < mhgt * .50f;
        int target = handle ? 2 : nativeHandle ? 1 : menuHandle ? 3 : moveTarget;
        bool other = spatialPointers[1 - hand].grasp;
        if (p.grasp && !wasGrasp[hand] && !other && singleGrabHand < 0 && p.gripValid &&
            (!s.questWallLocked || target != 0)) {
            singleGrabHand = hand;
            singleGrabTarget = target;
            singleFromGrip = p.gripPose;
            singleFromObject = target == 1   ? spatial.nativePose
                               : target == 2 ? spatial.pose
                               : target == 3 ? menuPanel
                               : fixed()     ? wallBase
                                             : worldPose();
            if (target == 0) {
                selectedWall = useSavedAnchor = false;
                automaticPlaced = true;
                s.questAnchor = 0;
            }
        }
        wasSpatialPress[hand] = press;
        wasGrasp[hand] = p.grasp;
    }
    if (singleGrabHand >= 0) {
        auto p = spatialPointers[singleGrabHand];
        if (!p.grasp || !p.gripValid || spatialPointers[1 - singleGrabHand].grasp)
            singleGrabHand = -1;
        else {
            auto pose = transform(p.gripPose, untransform(singleFromGrip, singleFromObject));
            if (!s.questMoveRotate)
                pose.orientation = singleFromObject.orientation;
            if (length(sub(pose.position, head.position)) < 50) {
                if (singleGrabTarget == 1)
                    spatial.nativePose = pose;
                else if (singleGrabTarget == 2)
                    spatial.pose = pose;
                else if (singleGrabTarget == 3) {
                    menuPanel = pose;
                    menuPlaced = true;
                } else if (fixed()) {
                    wallBase = pose;
                    wallTracked = !referenceLost;
                } else
                    panel = pose;
            }
        }
    }
    if (pending >= 0)
        spatialAction(pending, dt);
}
void menuPointer() {
    if (!app.menu)
        return;
    for (int i = 0; i < 2; ++i) {
        auto p = spatialPointers[i];
        if (!p.valid || uiConsumed[i])
            continue;
        auto pose = panelPose();
        auto size = physicalSize();
        float w = companion()
                      ? app.settings.questMenuDistance * .014f * app.settings.questOptionsScale * .01f
                      : size.width,
              h = companion() ? w * 9 / 16 : size.height;
        auto at = hitPlane(p, pose, 1);
        bool click = p.press;
        if (p.tipValid) {
            auto tip = rotate(inverse(pose.orientation), sub(p.tip, pose.position));
            if (tip.z > -.025f && tip.z < .08f && std::abs(tip.x) <= w / 2 &&
                std::abs(tip.y) <= h / 2) {
                at = {true, tip.x, tip.y, tip.z, 0};
                click = tip.z < .02f;
            }
        }
        if (at.hit && std::abs(at.x) <= w / 2 && std::abs(at.y) <= h / 2) {
            if (!pointerHit || click) {
                pointerHit = true;
                pointerX = (at.x / w + .5f) * width;
                pointerY = (.5f - at.y / h) * height;
                pointerClick = click && !grabbing && !blockPointerTrigger;
            }
        }
    }
}
bool spatialActive(int action) const {
    auto &s = app.settings;
    if (action >= PageModes && action <= PageNudge)
        return action - PageModes == deckPage;
    if (action >= ModeFirst && action < ModeFirst + 7)
        return mode() == action - ModeFirst;
    switch (action) {
    case Cinema:
        return mode() == 0 && !s.questAnchor && s.questTilt == 0;
    case Tabletop:
        return mode() == 0 && !s.questAnchor && s.questTilt == 55;
    case HeadFollow:
        return mode() == 0 && s.questAnchor == 1;
    case MoveGame:
        return moveTarget == 0;
    case MoveNative:
        return moveTarget == 1 || (mode() == 5 && moveTarget == 0);
    case MoveRemote:
        return moveTarget == 2;
    case MoveOptions:
        return moveTarget == 3;
    case LockPlacement:
        return s.questWallLocked;
    case ToggleAr:
        return s.questPassthrough;
    case ToggleTransparent:
        return s.questTransparent;
    case ToggleNative:
        return s.questInsetCrt;
    case ToggleGhosts:
        return s.enemyPreview;
    case ToggleObjects:
        return s.objectPreview;
    case ToggleBackdrop:
        return s.questWorldBackdrop;
    case ToggleCoplanar:
        return s.questCrtCoplanar;
    case ToggleHints:
        return s.questControlHints;
    case Refresh72:
        return s.questRefresh == 72;
    case Refresh90:
        return s.questRefresh == 90;
    case Refresh120:
        return s.questRefresh == 120;
    case DetailLow:
    case DetailBalanced:
    case DetailHigh:
        return s.questWallDetail == action - DetailLow;
    case EyeLow:
    case EyeBalanced:
    case EyeHigh:
        return s.questEyeDetail == action - EyeLow;
    default:
        return false;
    }
}
void renderDeckLabels() {
    if (!spatial.visible)
        return;
    auto &s = app.settings;
    uint64_t hash = 1469598103934665603ull;
    for (int n :
         {deckPage, moveTarget, app.settings.language, mode(), s.questScale, s.questRefresh,
          s.questRemoteScale, s.questInsetWidth, int(s.questTransparent), int(s.questPassthrough),
          int(s.questWallLocked), int(autoJoystick), int(app.menu)}) {
        hash ^= uint32_t(n);
        hash *= 1099511628211ull;
    }
    for (auto b : deckButtons) {
        hash ^= uint32_t(b.action) * 2 + unsigned(spatialActive(b.action));
        hash *= 1099511628211ull;
    }
    if (deckLabels && deckLabelHash == hash) {
        spatial.labels = deckLabels;
        return;
    }
    if (!deckLabels)
        deckLabels = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, 1024, 768);
    if (!deckLabels)
        return;
    auto target = SDL_GetRenderTarget(app.renderer);
    SDL_Rect viewport;
    SDL_RenderGetViewport(app.renderer, &viewport);
    float sx, sy;
    SDL_RenderGetScale(app.renderer, &sx, &sy);
    SDL_Rect clip;
    SDL_RenderGetClipRect(app.renderer, &clip);
    bool clipped = SDL_RenderIsClipEnabled(app.renderer);
    SDL_SetRenderTarget(app.renderer, deckLabels);
    SDL_RenderSetViewport(app.renderer, nullptr);
    SDL_RenderSetScale(app.renderer, 1, 1);
    SDL_RenderSetClipRect(app.renderer, nullptr);
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 0);
    SDL_RenderClear(app.renderer);
    ImDrawList list(ImGui::GetDrawListSharedData());
    list._ResetForNewFrame();
    list.PushTextureID(ImGui::GetIO().Fonts->TexID);
    list.PushClipRect({0, 0}, {1024, 768});
    auto font = ImGui::GetFont();
    auto text = [&](const char *label, float x, float y, float w, float h, float size = 24.f) {
        float wrap = w - 12;
        auto bound = font->CalcTextSizeA(size, FLT_MAX, wrap, label);
        if (bound.y > h - 8) {
            size = 18;
            bound = font->CalcTextSizeA(size, FLT_MAX, wrap, label);
        }
        list.AddText(font, size, {x + (w - bound.x) / 2, y + (h - bound.y) / 2},
                     IM_COL32(247, 250, 255, 255), label, nullptr, wrap);
    };
    std::string status = std::to_string(s.questRefresh) + " Hz  |  " +
                         std::to_string(s.questScale) + "%  |  " +
                         tr(moveTarget == 0   ? "remote_game"
                            : moveTarget == 1 ? "remote_native"
                            : moveTarget == 2 ? "remote_remote"
                                              : "remote_options",
                            app.settings.language);
    text(status.c_str(), 50, 28, 924, 50, 25);
    for (auto b : deckButtons) {
        float bw = b.width / spatialWidth * 1024, bh = b.height / spatialHeight * 768;
        float x = (b.x / spatialWidth + .5f) * 1024 - bw / 2,
              y = (.5f - b.y / spatialHeight) * 768 - bh / 2;
        std::string caption = tr(b.label, app.settings.language);
        if (spatialActive(b.action))
            caption = "[+] " + caption;
        text(caption.c_str(), x, y, bw, bh);
    }
    if (s.questControlHints)
        text(tr(autoJoystick ? "remote_hand_hint" : "remote_hint", app.settings.language), 20, 706,
             984, 58, 19);
    list.PopClipRect();
    list.PopTextureID();
    ImDrawData data;
    data.Valid = true;
    data.DisplayPos = {0, 0};
    data.DisplaySize = {1024, 768};
    data.FramebufferScale = {1, 1};
    data.AddDrawList(&list);
    ImGui_ImplSDLRenderer2_RenderDrawData(&data, app.renderer);
    SDL_RenderFlush(app.renderer);
    SDL_SetRenderTarget(app.renderer, target);
    SDL_RenderSetViewport(app.renderer, &viewport);
    SDL_RenderSetScale(app.renderer, sx, sy);
    SDL_RenderSetClipRect(app.renderer, clipped ? &clip : nullptr);
    SDL_SetTextureBlendMode(deckLabels, SDL_BLENDMODE_NONE);
    spatial.labels = deckLabels;
    deckLabelHash = hash;
}
