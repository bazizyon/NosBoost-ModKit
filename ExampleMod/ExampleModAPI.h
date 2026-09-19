#pragma once
#include <cstdint>

// Public API for other mods to call into ExampleMod, resolved via
// Host->GetModExport("ExampleMod", "CalculateExampleDamage").
struct ExampleDamageInput {
    uint32_t AttackPower;
    uint32_t DefensePower;
    float CriticalMultiplier;
};

using CalculateExampleDamageFn = uint32_t(*)(const ExampleDamageInput* Input);
