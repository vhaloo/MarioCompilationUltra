#include "quest_scene.h"
#ifdef MCU_QUEST
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <string>

namespace mcu {
namespace {
bool sameUuid(XrUuidEXT a, XrUuidEXT b) { return std::memcmp(a.data, b.data, sizeof a.data) == 0; }
bool label(const std::string &text, const char *value) {
    return ("," + text + ",").find("," + std::string(value) + ",") != std::string::npos;
}
} // namespace

struct QuestScene::Impl {
    fs::path file;
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    bool scenes = false, storage = false, capture = false;
    std::vector<Surface> surfaces;
    std::map<XrAsyncRequestIdFB, bool> queries;
    XrAsyncRequestIdFB createRequest = 0, saveRequest = 0, captureRequest = 0;
    XrSpace saved = XR_NULL_HANDLE, candidate = XR_NULL_HANDLE;
    XrUuidEXT candidateUuid{};
    XrExtent2Df bounds{3.2f, 2.5f}, candidateBounds{3.2f, 2.5f};
    bool saveCandidate = false;
    const char *message = "questsceneidle";
    std::chrono::steady_clock::time_point lastUpdate{};
    std::chrono::steady_clock::time_point queryStarted{}, anchorStarted{};
    PFN_xrQuerySpacesFB query = nullptr;
    PFN_xrRetrieveSpaceQueryResultsFB retrieve = nullptr;
    PFN_xrGetSpaceComponentStatusFB getStatus = nullptr;
    PFN_xrSetSpaceComponentStatusFB setStatus = nullptr;
    PFN_xrGetSpaceSemanticLabelsFB getLabels = nullptr;
    PFN_xrGetSpaceBoundingBox2DFB getBounds = nullptr;
    PFN_xrRequestSceneCaptureFB requestCapture = nullptr;
    PFN_xrCreateSpatialAnchorFB create = nullptr;
    PFN_xrSaveSpaceFB save = nullptr;

    explicit Impl(const fs::path &directory) : file(directory / "quest-wall-anchor.txt") {}
    template <class T> bool load(const char *name, T &out) {
        return XR_SUCCEEDED(xrGetInstanceProcAddr(instance, name,
                                                  reinterpret_cast<PFN_xrVoidFunction *>(&out))) &&
               out;
    }
    bool enable(XrSpace space, XrSpaceComponentTypeFB component) {
        XrSpaceComponentStatusFB state{XR_TYPE_SPACE_COMPONENT_STATUS_FB};
        if (!getStatus || XR_FAILED(getStatus(space, component, &state)))
            return false;
        if (state.enabled)
            return true;
        if (!state.changePending) {
            XrSpaceComponentStatusSetInfoFB request{XR_TYPE_SPACE_COMPONENT_STATUS_SET_INFO_FB};
            request.componentType = component;
            request.enabled = XR_TRUE;
            XrAsyncRequestIdFB id = 0;
            setStatus(space, &request, &id);
        }
        return false;
    }
    void clearCandidate() {
        if (candidate && candidate != saved)
            xrDestroySpace(candidate);
        candidate = XR_NULL_HANDLE;
        saveCandidate = false;
    }
    void querySpaces(bool restore) {
        if (!(restore ? storage : scenes) || !queries.empty() || createRequest || saveRequest)
            return;
        XrUuidEXT uuid{};
        if (restore) {
            std::ifstream input(file);
            std::string text;
            input >> text;
            if (text.size() != 32 ||
                text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                message = "questnosavedwall";
                return;
            }
            for (size_t i = 0; i < sizeof uuid.data; ++i)
                uuid.data[i] = static_cast<uint8_t>(std::stoul(text.substr(i * 2, 2), nullptr, 16));
            float w = 3.2f, h = 2.5f;
            if (input >> w >> h && std::isfinite(w) && std::isfinite(h) && w >= .25f && w <= 200 &&
                h >= .25f && h <= 20)
                bounds = {w, h};
        }
        XrSpaceStorageLocationFilterInfoFB local{XR_TYPE_SPACE_STORAGE_LOCATION_FILTER_INFO_FB};
        local.location = XR_SPACE_STORAGE_LOCATION_LOCAL_FB;
        XrSpaceComponentFilterInfoFB component{XR_TYPE_SPACE_COMPONENT_FILTER_INFO_FB};
        component.componentType = XR_SPACE_COMPONENT_TYPE_BOUNDED_2D_FB;
        XrSpaceUuidFilterInfoFB filter{XR_TYPE_SPACE_UUID_FILTER_INFO_FB};
        filter.uuidCount = 1;
        filter.uuids = &uuid;
        filter.next = &local;
        component.next = &local;
        XrSpaceQueryInfoFB request{XR_TYPE_SPACE_QUERY_INFO_FB};
        request.queryAction = XR_SPACE_QUERY_ACTION_LOAD_FB;
        request.maxResultCount = restore ? 1 : 256;
        request.timeout = 0;
        request.filter = restore ? reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB *>(&filter)
                                 : reinterpret_cast<XrSpaceFilterInfoBaseHeaderFB *>(&component);
        XrAsyncRequestIdFB id = 0;
        if (XR_SUCCEEDED(
                query(session, reinterpret_cast<XrSpaceQueryInfoBaseHeaderFB *>(&request), &id))) {
            queries[id] = restore;
            queryStarted = std::chrono::steady_clock::now();
            if (restore)
                anchorStarted = queryStarted;
            if (!restore) {
                for (auto &surface : surfaces)
                    xrDestroySpace(surface.space);
                surfaces.clear();
            }
            message = restore ? "questrestoringwall" : "questsceneloading";
        } else
            message = "questsceneunavailable";
    }
    void results(XrAsyncRequestIdFB id) {
        auto request = queries.find(id);
        if (request == queries.end())
            return;
        XrSpaceQueryResultsFB results{XR_TYPE_SPACE_QUERY_RESULTS_FB};
        if (XR_FAILED(retrieve(session, id, &results)) || results.resultCountOutput > 256)
            return;
        std::vector<XrSpaceQueryResultFB> values(results.resultCountOutput);
        results.resultCapacityInput = static_cast<uint32_t>(values.size());
        results.results = values.data();
        if (XR_FAILED(retrieve(session, id, &results)))
            return;
        values.resize(std::min<size_t>(values.size(), results.resultCountOutput));
        for (const auto &entry : values) {
            if (request->second) {
                clearCandidate();
                candidate = entry.space;
                candidateUuid = entry.uuid;
                saveCandidate = false;
                enable(candidate, XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB);
            } else {
                auto found = std::find_if(surfaces.begin(), surfaces.end(), [&](const Surface &s) {
                    return sameUuid(s.uuid, entry.uuid);
                });
                if (found == surfaces.end() && surfaces.size() < 256) {
                    Surface surface;
                    surface.space = entry.space;
                    surface.uuid = entry.uuid;
                    surfaces.push_back(surface);
                    enable(entry.space, XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB);
                } else if (found == surfaces.end() || found->space != entry.space)
                    xrDestroySpace(entry.space);
            }
        }
    }
    void update() {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastUpdate < std::chrono::milliseconds(250))
            return;
        lastUpdate = now;
        if (!queries.empty() && now - queryStarted > std::chrono::seconds(15)) {
            queries.clear();
            message = "questsceneunavailable";
        }
        if ((createRequest || candidate) && !saveRequest &&
            now - anchorStarted > std::chrono::seconds(20)) {
            createRequest = 0;
            clearCandidate();
            message = "questsavefailed";
        }
        for (auto &surface : surfaces) {
            if (surface.metadata)
                continue;
            enable(surface.space, XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB);
            XrSemanticLabelsFB labels{XR_TYPE_SEMANTIC_LABELS_FB};
            if (XR_FAILED(getLabels(session, surface.space, &labels)) ||
                labels.bufferCountOutput > 4096)
                continue;
            std::vector<char> text(labels.bufferCountOutput + 1, 0);
            labels.bufferCapacityInput = static_cast<uint32_t>(text.size());
            labels.buffer = text.data();
            if (XR_FAILED(getLabels(session, surface.space, &labels)))
                continue;
            const std::string names(text.data());
            surface.wall = label(names, "WALL_FACE") || label(names, "INNER_WALL_FACE");
            surface.floor = label(names, "FLOOR");
            if ((surface.wall || surface.floor) &&
                XR_SUCCEEDED(getBounds(session, surface.space, &surface.bounds)))
                surface.metadata =
                    surface.bounds.extent.width > .1f && surface.bounds.extent.height > .1f;
            else
                surface.metadata = true;
        }
        if (!candidate || saveRequest)
            return;
        if (!enable(candidate, XR_SPACE_COMPONENT_TYPE_LOCATABLE_FB))
            return;
        if (saveCandidate) {
            if (!enable(candidate, XR_SPACE_COMPONENT_TYPE_STORABLE_FB))
                return;
            XrSpaceSaveInfoFB request{XR_TYPE_SPACE_SAVE_INFO_FB};
            request.space = candidate;
            request.location = XR_SPACE_STORAGE_LOCATION_LOCAL_FB;
            request.persistenceMode = XR_SPACE_PERSISTENCE_MODE_INDEFINITE_FB;
            if (XR_FAILED(save(session, &request, &saveRequest))) {
                saveRequest = 0;
                clearCandidate();
                message = "questsavefailed";
            }
        } else {
            if (saved && saved != candidate)
                xrDestroySpace(saved);
            saved = candidate;
            candidate = XR_NULL_HANDLE;
            message = "questwallrestored";
        }
    }
    ~Impl() {
        clearCandidate();
        for (auto &surface : surfaces)
            if (surface.space)
                xrDestroySpace(surface.space);
        if (saved)
            xrDestroySpace(saved);
    }
};

QuestScene::QuestScene(const fs::path &dir) : impl(std::make_unique<Impl>(dir)) {}
QuestScene::~QuestScene() = default;
void QuestScene::initialise(XrInstance instance, XrSession session, bool scene, bool storage,
                            bool capture) {
    auto &p = *impl;
    p.instance = instance;
    p.session = session;
    bool base = (scene || storage) && p.load("xrGetSpaceComponentStatusFB", p.getStatus) &&
                p.load("xrSetSpaceComponentStatusFB", p.setStatus) &&
                p.load("xrQuerySpacesFB", p.query) &&
                p.load("xrRetrieveSpaceQueryResultsFB", p.retrieve);
    p.scenes = base && scene && p.load("xrGetSpaceSemanticLabelsFB", p.getLabels) &&
               p.load("xrGetSpaceBoundingBox2DFB", p.getBounds);
    p.storage = base && storage && p.load("xrCreateSpatialAnchorFB", p.create) &&
                p.load("xrSaveSpaceFB", p.save);
    p.capture = p.scenes && capture && p.load("xrRequestSceneCaptureFB", p.requestCapture);
}
void QuestScene::queryScene() { impl->querySpaces(false); }
void QuestScene::restoreAnchor() { impl->querySpaces(true); }
void QuestScene::captureScene() {
    auto &p = *impl;
    if (!p.capture || p.captureRequest || busy())
        return;
    XrSceneCaptureRequestInfoFB request{XR_TYPE_SCENE_CAPTURE_REQUEST_INFO_FB};
    if (XR_FAILED(p.requestCapture(p.session, &request, &p.captureRequest))) {
        p.captureRequest = 0;
        p.message = "questsceneunavailable";
    } else
        p.message = "questscenecapture";
}
void QuestScene::saveAnchor(XrSpace base, XrTime time, XrPosef pose, XrExtent2Df bounds) {
    auto &p = *impl;
    if (!p.storage || busy())
        return;
    XrSpatialAnchorCreateInfoFB request{XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_FB};
    request.space = base;
    request.time = time;
    request.poseInSpace = pose;
    p.candidateBounds = bounds;
    if (XR_FAILED(p.create(p.session, &request, &p.createRequest))) {
        p.createRequest = 0;
        p.message = "questsavefailed";
    } else {
        p.anchorStarted = std::chrono::steady_clock::now();
        p.message = "questwallsaving";
    }
}
void QuestScene::event(const XrEventDataBuffer &event) {
    auto &p = *impl;
    if (event.type == XR_TYPE_EVENT_DATA_SPACE_QUERY_RESULTS_AVAILABLE_FB) {
        p.results(
            reinterpret_cast<const XrEventDataSpaceQueryResultsAvailableFB &>(event).requestId);
    } else if (event.type == XR_TYPE_EVENT_DATA_SPACE_QUERY_COMPLETE_FB) {
        auto &value = reinterpret_cast<const XrEventDataSpaceQueryCompleteFB &>(event);
        auto found = p.queries.find(value.requestId);
        if (found != p.queries.end()) {
            p.message = XR_FAILED(value.result) ? "questsceneunavailable"
                        : found->second
                            ? (p.candidate ? "questrestoringwall" : "questnosavedwall")
                            : (p.surfaces.empty() ? "questscenenone" : "questsceneready");
            p.queries.erase(found);
        }
    } else if (event.type == XR_TYPE_EVENT_DATA_SCENE_CAPTURE_COMPLETE_FB) {
        auto &value = reinterpret_cast<const XrEventDataSceneCaptureCompleteFB &>(event);
        if (value.requestId == p.captureRequest) {
            p.captureRequest = 0;
            if (XR_SUCCEEDED(value.result))
                p.querySpaces(false);
            else
                p.message = "questsceneunavailable";
        }
    } else if (event.type == XR_TYPE_EVENT_DATA_SPATIAL_ANCHOR_CREATE_COMPLETE_FB) {
        auto &value = reinterpret_cast<const XrEventDataSpatialAnchorCreateCompleteFB &>(event);
        if (value.requestId == p.createRequest) {
            p.createRequest = 0;
            if (XR_SUCCEEDED(value.result)) {
                p.clearCandidate();
                p.candidate = value.space;
                p.candidateUuid = value.uuid;
                p.saveCandidate = true;
            } else
                p.message = "questsavefailed";
        } else if (XR_SUCCEEDED(value.result) && value.space)
            xrDestroySpace(value.space);
    } else if (event.type == XR_TYPE_EVENT_DATA_SPACE_SAVE_COMPLETE_FB) {
        auto &value = reinterpret_cast<const XrEventDataSpaceSaveCompleteFB &>(event);
        if (value.requestId == p.saveRequest) {
            p.saveRequest = 0;
            if (XR_SUCCEEDED(value.result)) {
                constexpr char hex[] = "0123456789abcdef";
                std::string text;
                for (auto byte : value.uuid.data) {
                    text += hex[byte >> 4];
                    text += hex[byte & 15];
                }
                text += '\n';
                text += std::to_string(p.candidateBounds.width) + " " +
                        std::to_string(p.candidateBounds.height) + "\n";
                try {
                    atomicWrite(p.file, std::vector<uint8_t>(text.begin(), text.end()));
                    if (p.saved && p.saved != p.candidate)
                        xrDestroySpace(p.saved);
                    p.saved = p.candidate;
                    p.bounds = p.candidateBounds;
                    p.candidate = XR_NULL_HANDLE;
                    p.saveCandidate = false;
                    p.message = "questwallsaved";
                } catch (...) {
                    p.clearCandidate();
                    p.message = "questsavefailed";
                }
            } else {
                p.clearCandidate();
                p.message = "questsavefailed";
            }
        }
    }
}
void QuestScene::update() { impl->update(); }
const std::vector<QuestScene::Surface> &QuestScene::surfaces() const { return impl->surfaces; }
XrSpace QuestScene::anchor() const { return impl->saved; }
XrExtent2Df QuestScene::anchorBounds() const { return impl->bounds; }
bool QuestScene::supported() const { return impl->scenes; }
bool QuestScene::storageSupported() const { return impl->storage; }
bool QuestScene::captureSupported() const { return impl->capture; }
bool QuestScene::busy() const {
    return !impl->queries.empty() || impl->createRequest || impl->saveRequest ||
           impl->captureRequest || impl->candidate;
}
const char *QuestScene::status() const { return impl->message; }
} // namespace mcu
#endif
