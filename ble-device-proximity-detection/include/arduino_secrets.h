// Fill in  your WiFi networks SSID and password
#define WIFI_SSID "LabNet"
#define WIFI_PASS "********"

// Fill in the hostname of your AWS IoT broker
// Do not add protocol prefix (e.g. mqtt:// or http://)
// Should look something like
// #define AWS_IOT_ENDPOINT_ADDRESS "asdf12345-ats.iot.us-west-2.amazonaws.com"
#define AWS_IOT_ENDPOINT_ADDRESS "**************-ats.iot.us-west-2.amazonaws.com"

// The ATECC608 TNG Device Certificate.
// Should look something like
// const char THING_CERTIFICATE[] = R"(
// -----BEGIN CERTIFICATE-----
// LOTSOFALPHANUMERICCHARACTERS==
// -----END CERTIFICATE-----
// )";
const char THING_CERTIFICATE[] = R"(
-----BEGIN CERTIFICATE-----
M***Hz****Wg*w***g*QQmZxY*****8M+M**V*/S+z***ggqh*jOPQQ**j*PMS*w
HwY*VQQ***hN*WNy**No*X*gVGVj*G*v*G9n*S*J*mMx*j*o*gNV**MM*UNy*X*0
*y***XRoZW*0*WNh*G*v*****W*uZX*gM**wM**g*w0yM**yM*UwNj*wM***G*8y
M*Q*M**wN***M**wM*owQj*hM*8G**U**gwY*W*j*m9j*G*w**R*Y*hu**xvZ**g
SW*jMR0wGwY*VQQ***Rz*j*xMjN*N*R*M***NUZ*Q**wM**ZM*MG*yqGSM49*g*G
**qGSM49*w*H*0***OH*pU*     EXAMPLE     **M*g***wGp9*Z*+4*8**O*X
*SHV*g6uY*6UVn*hgxV**Zp**y**Q8y**z**h**jgY0wgYow*gY*VR0R**Mw**Q*
M*0xGz*Z*gNV**U**mV***Q4X0U4RU*xQj*4MjZ*M**M*gNVHRM***8**j**M*4G
**U**w**/wQ**w*******gNVHQ4**gQUhq*W*XgSVxr****6**mXq+**Vj*wHwY*
VR0j**gw*o*UVy*Q*mVQ**m*qJYS**vMRJPnrg*w*gY**oZ*zj0**w**S**wRQ*g
GnP+0Qq*ph*gPq*Xx*J**u*4+g6grH*4QuW*Hh**WVM**Q*g*6*u***QQ*j*V**p
9P****/**/Yg*8***n*6**9Zp*==
-----END CERTIFICATE-----
)";