#include <cstdlib>
#include "power_manage.h"

void Power::reset()
{
    system("reboot");
}