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

/* Display */
#define IDC_PAGE_DISPLAY 1200
#define IDC_MODE         1201
#define IDC_SCALE        1202
#define IDC_PRESET       1204
#define IDC_PRESET_LABEL 1205
#define IDC_SCALE_LABEL  1206
#define IDC_RESOLUTION   1207
#define IDC_RENDERER     1209
#define IDC_FRAMES       1212
#define IDC_VSYNC        1213
#define IDC_RENDERER_LABEL 1214
#define IDC_COLOUR_LABEL 1219
#define IDC_COLOUR       1220
#define IDC_DISPLAY_GROUP 1224
#define IDC_MAXLIMIT     1226
#define IDC_MAXLIMIT_LABEL 1227
#define IDC_WIDESCREEN   1229
#define IDC_ASPECT       1230
#define IDC_ASPECT_LABEL 1231

/* Renderer */
#define IDC_PAGE_RENDERER 1300
#define IDC_INTERP       1301
#define IDC_SMOOTHING    1302
#define IDC_GRADIENT_GROUP 1303
#define IDC_SMOOTH_SKY   1304
#define IDC_SMOOTH_SHADING 1305
#define IDC_WORLD_GROUP  1306
#define IDC_DRAWDIST     1307
#define IDC_DRAWDIST_LABEL 1308
#define IDC_TEXTURES     1309
#define IDC_TEXTURES_LABEL 1310
#define IDC_LIGHTING     1311
#define IDC_LIGHTING_LABEL 1312
#define IDC_DETAIL       1313
#define IDC_DETAIL_LABEL 1314
#define IDC_MISC_GROUP   1315
#define IDC_COLOUR_ANIM  1316
#define IDC_HUD43        1317
#define IDC_HUDSIZE      1318
#define IDC_HUDSIZE_LABEL 1319
#define IDC_WIDEMENUS    1320
#define IDC_FIXES_GROUP  1321
#define IDC_GROUND_PERSP 1322
#define IDC_SORT_FOOT    1323
#define IDC_CLOSE_GAPS   1324
#define IDC_HORIZON      1325

/* Audio */
#define IDC_PAGE_AUDIO   1400
#define IDC_MUSIC        1401
#define IDC_VOLUME       1402
#define IDC_FOLDER       1403
#define IDC_BROWSE       1404
#define IDC_TRACKS       1405
#define IDC_VOLUME_LABEL 1406
#define IDC_FOLDER_LABEL 1407
#define IDC_SFX_GROUP    1408
#define IDC_DISTANCE_VOL 1409
#define IDC_STEREO_PAN   1410
#define IDC_CHANNELS     1411
#define IDC_CHANNELS_LABEL 1412

/* Controller */
#define IDC_PAGE_PAD     1500
#define IDC_PAD          1501
#define IDC_DEADZONE     1502
#define IDC_SENSITIVITY  1503
#define IDC_LAYOUT       1504
#define IDC_PAD_GROUP    1505
#define IDC_PAD_L1       1506
#define IDC_PAD_L2       1507
#define IDC_PAD_L3       1508
#define IDC_JOYCPL       1509
#define IDC_PAD_NOTE     1510
#define IDC_VIBRATION    1511
#define IDC_NITRO_RUMBLE 1512
#define IDC_VIB_GROUP    1513
#define IDC_INTENSITY_LABEL 1514
#define IDC_INTENSITY    1515
#define IDC_MENU_GROUP   1516
#define IDC_MENU_NAV     1517

/* Advanced */
#define IDC_PAGE_ADVANCED 1600
#define IDC_LANGUAGE     1601
#define IDC_RATE         1602
#define IDC_CAP          1603
#define IDC_LOG          1604

/* Updates */
#define IDC_PAGE_UPDATES 1700
#define IDC_VERSION      1701
#define IDC_RELEASES     1702

/* under the tabs, always shown */
#define IDC_SKIP         1050
#define IDC_SAVE         1051
#define IDC_DOSPATCH     1052
#define IDC_HELPFILE     1053
