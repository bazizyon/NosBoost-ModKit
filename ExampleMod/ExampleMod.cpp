#include "../ModContract.h"
#include "ExampleModAPI.h"
#include "TLBSWidget.h"
#include "TEWLabel.h"
#include "LevPacket.h"

#include <chrono>
#include <cstdio>

// An example mod that
// receives level packets,
// tracks the time since start-up
// displays an ImGUI window with information.
namespace {
    bool WindowVisible = false;
    std::chrono::steady_clock::time_point StartTime;

    int LevPacketCount = 0;
    bool HasReceivedLevPacket = false;
    Packet::LevPacket LatestLevPacket;
    const ModHost* CachedHost = nullptr;

    // There is no need to handwrite the requirements, they can be linked to the SDK straight.
    constexpr ModClassRequirement Requirements[] = {
        {TLBSWidget::ClassName, TLBSWidget::Version, TLBSWidget::ExpectedSize},
    };

    // "SomeOtherMod" isn't a real mod -- this always resolves null, which is
    // the point: it shows the resolve-and-report pattern below.
    using GetSomeOtherValueFn = int(*)();

    // The function that will be linked to a packet receive call.
    bool OnLevPacket(const Packet::LevPacket& Packet) {
        LevPacketCount++;
        HasReceivedLevPacket = true;
        LatestLevPacket = Packet;

        // Example: overriding a received packet instead of just observing it.
        // Copy it, change a field, hand the copy to the runtime to serialize
        // and inject, then suppress the real one by returning false.
        //
        // Packet::LevPacket Replacement = Packet;
        // Replacement.battleLevel = 99;
        // Packet::InjectReceivePacket(CachedHost, Replacement);
        // return false;

        return true;
    }
}

// DLLExport so that runtime can find these exports via GetProcAdress
extern "C" {
    // Writes the size of "Requirements" onto OutCount and returns the array of requirements.
    // This is due to requirements just being returned as a raw pointer and the runtime not knowing how many elements to read.
    __declspec(dllexport) const ModClassRequirement* ModGetRequirements(size_t* OutCount) {
        *OutCount = std::size(Requirements);
        return Requirements;
    }

    // Mod startup export
    // ImGUI doesn't have to be used, however it still has to be linked against. The ImGUI values can just be ignored.
    // Subscription to packets should all be done here.
    __declspec(dllexport) void ModStartup(
        ImGuiContext* Context, const ImGuiMemAllocFunc AllocFunc, const ImGuiMemFreeFunc FreeFunc,
        void* AllocUserData, const ModHost* Host
    ) {
        ImGui::SetCurrentContext(Context);
        ImGui::SetAllocatorFunctions(AllocFunc, FreeFunc, AllocUserData);
        StartTime = std::chrono::steady_clock::now();
        CachedHost = Host;
        Packet::SubscribePacket(Host, &OnLevPacket);
    }

    // Mod shutdown export, can be used to clean leftover widgets and undo changes on native widgets.
    __declspec(dllexport) void ModShutdown() {

    }

    // Mod Tick is called every EndFrame.
    // Every widget is under RootWidget.
    __declspec(dllexport) void ModTick(const TLBSWidget* RootWidget, const TickContext tickContext) {
        // Example: calling another mod's export and reporting our own
        // status based on whether it's there. Resolved fresh every tick, never cached
        // if (CachedHost) {
        //     const auto GetSomeOtherValue = reinterpret_cast<GetSomeOtherValueFn>(
        //         CachedHost->GetModExport("SomeOtherMod", "GetSomeOtherValue"));
        //     if (GetSomeOtherValue) {
        //         CachedHost->ReportStatus(ModHealthLevel::Ok, "");
        //     } else {
        //         CachedHost->ReportStatus(ModHealthLevel::Broken, "SomeOtherMod not found.");
        //     }
        // }

        // Example: moving a widget.
        // if (RootWidget && RootWidget->childrenList && RootWidget->childrenList->count > 0) {
        //     TLBSWidget* FirstChild = RootWidget->childrenList->list[0];
        //     FirstChild->MoveTo(100, 100);
        // }

        // Example: creating a widget and attaching it to the tree.
        // WARNING: don't implement such a function in tick, leading to creating a widget every frame. x)
        //
        // TEWLabel* NewLabel = Widget::Create<TEWLabel>(CachedHost);
        // if (NewLabel && RootWidget) {
        //     NewLabel->parent = const_cast<TLBSWidget*>(RootWidget);
        //     RootWidget->childrenList->push_back(NewLabel);
        // }

        if (!WindowVisible) return;

        const auto ElapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - StartTime).count();

        char Buffer[64];
        snprintf(Buffer, sizeof(Buffer), "Session time: %02lld:%02lld:%02lld",
                 static_cast<long long>(ElapsedSeconds / 3600),
                 static_cast<long long>(ElapsedSeconds / 60 % 60),
                 static_cast<long long>(ElapsedSeconds % 60));

        ImGui::Begin("Example Mod", &WindowVisible);
        ImGui::TextUnformatted(Buffer);

        ImGui::Text("Lev packets received: %d", LevPacketCount);
        if (HasReceivedLevPacket) {
            ImGui::Text("Last lev packet -- battle level %d (%ld/%ld xp), job level %d (%ld/%ld xp), hero level %d (%ld/%ld xp)",
                        LatestLevPacket.battleLevel, LatestLevPacket.battleLevelXP, LatestLevPacket.battleLevelXPMax,
                        LatestLevPacket.jobLevel, LatestLevPacket.jobLevelXP, LatestLevPacket.jobLevelXPMax,
                        LatestLevPacket.heroLevel, LatestLevPacket.heroLevelXP, LatestLevPacket.heroLevelXPMax);
        } else {
            ImGui::TextUnformatted("No lev packet received yet.");
        }

        ImGui::End();
    }

    // The mod menu (F9) lists buttons for every mod loaded.
    // Upon press of their respective button, a mod's ToggleMainWindow is called.
    // Preferably, this function should be used to toggle the mod's ImGUI/Game Widget, not a keybind.
    __declspec(dllexport) void ModToggleMainWindow() {
        WindowVisible = !WindowVisible;
    }

    // Exported for other mods to call via Host->GetModExport("ExampleMod", "CalculateExampleDamage").
    __declspec(dllexport) uint32_t  CalculateExampleDamage(const ExampleDamageInput* Input) {
        if (!Input) return 0;
        const uint32_t Base = Input->AttackPower > Input->DefensePower ? Input->AttackPower - Input->DefensePower : 0;
        return static_cast<uint32_t>(Base * Input->CriticalMultiplier);
    }

    // Example: another mod calling this export. Resolve lazily where it's
    // actually used (ModTick, a button press), never cache it.
    //
    // #include "ExampleModAPI.h" // ExampleMod's own public header
    //
    // const auto CalculateExampleDamage = reinterpret_cast<CalculateExampleDamageFn>(
    //     Host->GetModExport("ExampleMod", "CalculateExampleDamage"));
    // if (CalculateExampleDamage) {
    //     const ExampleDamageInput Input{ /*AttackPower*/ 50, /*DefensePower*/ 20, /*CriticalMultiplier*/ 1.5f };
    //     const uint32_t Damage = CalculateExampleDamage(&Input);
    // } else {
    //     CachedHost->ReportStatus(ModHealthLevel::Broken, "DamageCalculator was not found");
    // or
    //     CachedHost->ReportStatus(ModHealthLevel::Warning, "DamageCalculator was not found.");
    // if the inexistance of the given mod causes missing features and not a complete break.
    // }
}
