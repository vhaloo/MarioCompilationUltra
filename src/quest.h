#pragma once
#ifdef MCU_QUEST
#include <memory>
namespace mcu {
class App;
struct Settings;
// Android-only presenter. Simulation and the original SDL menu remain in App.
class QuestPresentation {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit QuestPresentation(App &app);
    ~QuestPresentation();
    void initialise();
    bool beginFrame();
    void endFrame();
    bool needsRedraw() const;
    void prepareRender();
    void rendered();
    void configureRender(Settings &settings) const;
    bool simulationPaused() const;
    void prepareUi();
    void drawReticle();
    void options();
    void recenter();
    void chooseRom();
    void logicalSize(int &width, int &height) const;
};
} // namespace mcu
#endif
