#include "ecu_scenarios.h"

int main(void)
{
    return ecu_run_scenarios(stdout, UINT32_C(0xB17));
}
