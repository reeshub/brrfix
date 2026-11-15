/* Control and resource numbers for the launcher's settings window. Each tab's controls are
 * numbered in its own hundred (launcher.c, page_of): a label or frame with no ID of its own
 * takes its tab's first number, so the tabs can show and hide it too. */
#define IDI_APP          1
#define IDD_SETTINGS     100

#define IDC_TABS         1001

/* General */
#define IDC_PAGE_GENERAL 1100
#define IDC_TITLE        1101
#define IDC_PERIOD       1102
#define IDC_MODERN       1103

/* Video */
#define IDC_PAGE_VIDEO   1200
#define IDC_MODE         1201
#define IDC_SCALE        1202
#define IDC_ASPECT       1203
#define IDC_PRESET       1204
#define IDC_PRESET_LABEL 1205
#define IDC_SCALE_LABEL  1206
#define IDC_RESOLUTION   1207
#define IDC_RENDERER     1209
#define IDC_INTERP       1210
#define IDC_SMOOTHING    1211
#define IDC_FRAMES       1212
#define IDC_VSYNC        1213
#define IDC_RENDERER_LABEL 1214
#define IDC_RENDERRES    1215
#define IDC_RENDERRES_LABEL 1216
#define IDC_DRAWDIST     1217

/* Audio */
#define IDC_PAGE_AUDIO   1300
#define IDC_MUSIC        1301
#define IDC_VOLUME       1302
#define IDC_FOLDER       1303
#define IDC_BROWSE       1304
#define IDC_TRACKS       1305
#define IDC_VOLUME_LABEL 1306
#define IDC_FOLDER_LABEL 1307

/* Controller */
#define IDC_PAGE_PAD     1400
#define IDC_PAD          1401
#define IDC_DEADZONE     1402
#define IDC_SENSITIVITY  1403
#define IDC_LAYOUT       1404
#define IDC_PAD_GROUP    1405
#define IDC_PAD_L1       1406
#define IDC_PAD_L2       1407
#define IDC_PAD_L3       1408
#define IDC_JOYCPL       1409
#define IDC_PAD_NOTE     1410
#define IDC_VIBRATION    1411
#define IDC_NITRO_RUMBLE 1412
#define IDC_VIB_GROUP    1413
#define IDC_INTENSITY_LABEL 1414
#define IDC_INTENSITY    1415

/* Advanced */
#define IDC_PAGE_ADVANCED 1500
#define IDC_LANGUAGE     1501
#define IDC_RATE         1502
#define IDC_CAP          1503
#define IDC_LOG          1504

/* under the tabs, always shown */
#define IDC_DEFAULTS     1043
#define IDC_SKIP         1050
#define IDC_SAVE         1051
#define IDC_DOSPATCH     1052
#define IDC_HELPFILE     1053
