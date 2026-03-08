# 1 "C:\\Users\\Denis\\AppData\\Local\\Temp\\tmpo1yrx7hy"
#include <Arduino.h>
# 1 "C:/msys64/home/Denis/plc-esp2/src/fcplc.ino"
# 12 "C:/msys64/home/Denis/plc-esp2/src/fcplc.ino"
#include "app.hpp"

App app;
void setup();
void loop();
#line 16 "C:/msys64/home/Denis/plc-esp2/src/fcplc.ino"
void setup()
{
    app.begin();
}

void loop()
{
    app.loop();
}