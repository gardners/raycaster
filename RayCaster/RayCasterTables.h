#include "RayCaster.h"

#define HAS_TABLES

const uint16_t g_tan[256] = {
    0,    1,    3,    4,    6,    7,    9,    11,   12,   14,   15,    17,    18,    20,   22,   23,   25,   26,   28,   29,   31,   33,
    34,   36,   37,   39,   41,   42,   44,   46,   47,   49,   50,    52,    54,    55,   57,   59,   60,   62,   64,   65,   67,   69,
    70,   72,   74,   75,   77,   79,   81,   82,   84,   86,   88,    89,    91,    93,   95,   96,   98,   100,  102,  104,  106,  107,
    109,  111,  113,  115,  117,  119,  121,  123,  124,  126,  128,   130,   132,   134,  136,  138,  140,  142,  145,  147,  149,  151,
    153,  155,  157,  159,  162,  164,  166,  168,  171,  173,  175,   177,   180,   182,  185,  187,  189,  192,  194,  197,  199,  202,
    204,  207,  210,  212,  215,  218,  220,  223,  226,  229,  232,   234,   237,   240,  243,  246,  249,  252,  255,  259,  262,  265,
    268,  272,  275,  278,  282,  285,  289,  293,  296,  300,  304,   308,   311,   315,  319,  323,  328,  332,  336,  340,  345,  349,
    354,  358,  363,  368,  373,  378,  383,  388,  393,  398,  404,   409,   415,   421,  427,  433,  439,  445,  451,  458,  465,  471,
    478,  486,  493,  500,  508,  516,  524,  532,  541,  549,  558,   568,   577,   587,  597,  607,  618,  628,  640,  651,  663,  675,
    688,  701,  715,  729,  744,  759,  774,  791,  808,  825,  843,   862,   882,   903,  925,  947,  971,  996,  1022, 1049, 1077, 1108,
    1140, 1173, 1209, 1246, 1286, 1329, 1374, 1423, 1475, 1531, 1591,  1655,  1725,  1801, 1884, 1975, 2075, 2185, 2308, 2445, 2599, 2773,
    2972, 3202, 3470, 3787, 4166, 4631, 5210, 5956, 6950, 8341, 10428, 13905, 20859, 0xA2F8};

const uint16_t g_cotan[256] = {
    0,    0xA2F8, 20859, 13905, 10428, 8341, 6950, 5956, 5210, 4631, 4166, 3787, 3470, 3202, 2972, 2773, 2599, 2445, 2308, 2185, 2075, 1975,
    1884, 1801,  1725,  1655,  1591,  1531, 1475, 1423, 1374, 1329, 1286, 1246, 1209, 1173, 1140, 1108, 1077, 1049, 1022, 996,  971,  947,
    925,  903,   882,   862,   843,   825,  808,  791,  774,  759,  744,  729,  715,  701,  688,  675,  663,  651,  640,  628,  618,  607,
    597,  587,   577,   568,   558,   549,  541,  532,  524,  516,  508,  500,  493,  486,  478,  471,  465,  458,  451,  445,  439,  433,
    427,  421,   415,   409,   404,   398,  393,  388,  383,  378,  373,  368,  363,  358,  354,  349,  345,  340,  336,  332,  328,  323,
    319,  315,   311,   308,   304,   300,  296,  293,  289,  285,  282,  278,  275,  272,  268,  265,  262,  259,  256,  252,  249,  246,
    243,  240,   237,   234,   232,   229,  226,  223,  220,  218,  215,  212,  210,  207,  204,  202,  199,  197,  194,  192,  189,  187,
    185,  182,   180,   177,   175,   173,  171,  168,  166,  164,  162,  159,  157,  155,  153,  151,  149,  147,  145,  142,  140,  138,
    136,  134,   132,   130,   128,   126,  124,  123,  121,  119,  117,  115,  113,  111,  109,  107,  106,  104,  102,  100,  98,   96,
    95,   93,    91,    89,    88,    86,   84,   82,   81,   79,   77,   75,   74,   72,   70,   69,   67,   65,   64,   62,   60,   59,
    57,   55,    54,    52,    50,    49,   47,   46,   44,   42,   41,   39,   37,   36,   34,   33,   31,   29,   28,   26,   25,   23,
    22,   20,    18,    17,    15,    14,   12,   11,   9,    7,    6,    4,    3,    1};

const uint8_t g_sin[256] = {
    0,   1,   3,   4,   6,   7,   9,   10,  12,  14,  15,  17,  18,  20,  21,  23,  25,  26,  28,  29,  31,  32,  34,  36,  37,  39,
    40,  42,  43,  45,  46,  48,  49,  51,  53,  54,  56,  57,  59,  60,  62,  63,  65,  66,  68,  69,  71,  72,  74,  75,  77,  78,
    80,  81,  83,  84,  86,  87,  89,  90,  92,  93,  95,  96,  97,  99,  100, 102, 103, 105, 106, 108, 109, 110, 112, 113, 115, 116,
    117, 119, 120, 122, 123, 124, 126, 127, 128, 130, 131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 144, 146, 147, 148, 149, 151,
    152, 153, 155, 156, 157, 158, 159, 161, 162, 163, 164, 166, 167, 168, 169, 170, 171, 173, 174, 175, 176, 177, 178, 179, 181, 182,
    183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208,
    209, 210, 211, 211, 212, 213, 214, 215, 216, 217, 217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 225, 226, 227, 227, 228, 229,
    230, 230, 231, 232, 232, 233, 234, 234, 235, 235, 236, 237, 237, 238, 238, 239, 239, 240, 241, 241, 242, 242, 243, 243, 244, 244,
    244, 245, 245, 246, 246, 247, 247, 247, 248, 248, 249, 249, 249, 250, 250, 250, 251, 251, 251, 251, 252, 252, 252, 252, 253, 253,
    253, 253, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

const uint8_t g_cos[256] = {
    0,   255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254, 253, 253, 253, 253, 252,
    252, 252, 252, 251, 251, 251, 251, 250, 250, 250, 249, 249, 249, 248, 248, 247, 247, 247, 246, 246, 245, 245, 244, 244, 244, 243,
    243, 242, 242, 241, 241, 240, 239, 239, 238, 238, 237, 237, 236, 235, 235, 234, 234, 233, 232, 232, 231, 230, 230, 229, 228, 227,
    227, 226, 225, 225, 224, 223, 222, 221, 221, 220, 219, 218, 217, 217, 216, 215, 214, 213, 212, 211, 211, 210, 209, 208, 207, 206,
    205, 204, 203, 202, 201, 200, 199, 198, 197, 196, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 185, 184, 183, 182, 181, 179,
    178, 177, 176, 175, 174, 173, 171, 170, 169, 168, 167, 166, 164, 163, 162, 161, 159, 158, 157, 156, 155, 153, 152, 151, 149, 148,
    147, 146, 144, 143, 142, 140, 139, 138, 136, 135, 134, 132, 131, 130, 128, 127, 126, 124, 123, 122, 120, 119, 117, 116, 115, 113,
    112, 110, 109, 108, 106, 105, 103, 102, 100, 99,  97,  96,  95,  93,  92,  90,  89,  87,  86,  84,  83,  81,  80,  78,  77,  75,
    74,  72,  71,  69,  68,  66,  65,  63,  62,  60,  59,  57,  56,  54,  53,  51,  49,  48,  46,  45,  43,  42,  40,  39,  37,  36,
    34,  32,  31,  29,  28,  26,  25,  23,  21,  20,  18,  17,  15,  14,  12,  10,  9,   7,   6,   4,   3,   1};

const uint8_t g_nearHeight[256] = {
    130, 127, 125, 122, 120, 117, 115, 113, 111, 109, 107, 105, 103, 101, 100, 98, 96, 95, 93, 92, 90, 89, 88, 86, 85, 84, 83, 82, 81,
    80,  78,  77,  76,  75,  75,  74,  73,  72,  71,  70,  69,  68,  68,  67,  66, 65, 65, 64, 63, 63, 62, 61, 61, 60, 60, 59, 58, 58,
    57,  57,  56,  56,  55,  55,  54,  54,  53,  53,  52,  52,  51,  51,  50,  50, 50, 49, 49, 48, 48, 48, 47, 47, 46, 46, 46, 45, 45,
    45,  44,  44,  44,  43,  43,  43,  42,  42,  42,  41,  41,  41,  41,  40,  40, 40, 40, 39, 39, 39, 38, 38, 38, 38, 37, 37, 37, 37,
    37,  36,  36,  36,  36,  35,  35,  35,  35,  35,  34,  34,  34,  34,  34,  33, 33, 33, 33, 33, 32, 32, 32, 32, 32, 32, 31, 31, 31,
    31,  31,  31,  30,  30,  30,  30,  30,  30,  30,  29,  29,  29,  29,  29,  29, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27,
    27,  27,  27,  26,  26,  26,  26,  26,  26,  26,  26,  25,  25,  25,  25,  25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24,
    24,  24,  23,  23,  23,  23,  23,  23,  23,  23,  23,  23,  22,  22,  22,  22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21,
    21,  21,  21,  21,  21,  21,  21,  21,  20,  20,  20,  20,  20,  20,  20,  20, 20, 20, 20, 20, 20, 20, 20, 19};

const uint8_t g_farHeight[256] = {
    150, 125, 107, 93, 83, 75, 68, 62, 57, 53, 50, 46, 44, 41, 39, 37, 35, 34, 32, 31, 30, 28, 27, 26, 25, 25, 24, 23, 22, 22, 21, 20,
    20,  19,  19,  18, 18, 17, 17, 17, 16, 16, 15, 15, 15, 15, 14, 14, 14, 13, 13, 13, 13, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11,
    10,  10,  10,  10, 10, 10, 10, 9,  9,  9,  9,  9,  9,  9,  9,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  7,  7,  7,  7,  7,  7,  7,
    7,   7,   7,   7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  5,  5,
    5,   5,   5,   5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,   4,   4,   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,   3,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,   3,   3,   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2};

const uint16_t g_nearStep[256] = {
    255,  260,  266,  271,  277,  282,  288,  293,  299,  304,  309,  315,  320,  326,  331,  337,  342,  348,  353,  359,  364,  370,
    375,  380,  386,  391,  397,  402,  408,  413,  419,  424,  430,  435,  441,  446,  451,  457,  462,  468,  473,  479,  484,  490,
    495,  501,  506,  512,  517,  522,  528,  533,  539,  544,  550,  555,  561,  566,  572,  577,  582,  588,  593,  599,  604,  610,
    615,  621,  626,  632,  637,  643,  648,  653,  659,  664,  670,  675,  681,  686,  692,  697,  703,  708,  714,  719,  724,  730,
    735,  741,  746,  752,  757,  763,  768,  774,  779,  785,  790,  795,  801,  806,  812,  817,  823,  828,  834,  839,  845,  850,
    856,  861,  866,  872,  877,  883,  888,  894,  899,  905,  910,  916,  921,  927,  932,  937,  943,  948,  954,  959,  965,  970,
    976,  981,  987,  992,  998,  1003, 1008, 1014, 1019, 1025, 1030, 1036, 1041, 1047, 1052, 1058, 1063, 1069, 1074, 1079, 1085, 1090,
    1096, 1101, 1107, 1112, 1118, 1123, 1129, 1134, 1140, 1145, 1150, 1156, 1161, 1167, 1172, 1178, 1183, 1189, 1194, 1200, 1205, 1211,
    1216, 1221, 1227, 1232, 1238, 1243, 1249, 1254, 1260, 1265, 1271, 1276, 1282, 1287, 1292, 1298, 1303, 1309, 1314, 1320, 1325, 1331,
    1336, 1342, 1347, 1353, 1358, 1363, 1369, 1374, 1380, 1385, 1391, 1396, 1402, 1407, 1413, 1418, 1424, 1429, 1434, 1440, 1445, 1451,
    1456, 1462, 1467, 1473, 1478, 1484, 1489, 1495, 1500, 1505, 1511, 1516, 1522, 1527, 1533, 1538, 1544, 1549, 1555, 1560, 1566, 1571,
    1576, 1582, 1587, 1593, 1598, 1604, 1609, 1615, 1620, 1626, 1631, 1637, 1642, 1647};

const uint16_t g_farStep[256] = {
    255,   299,   342,   386,   430,   473,   517,   561,   604,   648,   692,   735,   779,   823,   866,   910,   954,   998,   1041,
    1085,  1129,  1172,  1216,  1260,  1303,  1347,  1391,  1434,  1478,  1522,  1566,  1609,  1653,  1697,  1740,  1784,  1828,  1871,
    1915,  1959,  2002,  2046,  2090,  2134,  2177,  2221,  2265,  2308,  2352,  2396,  2439,  2483,  2527,  2570,  2614,  2658,  2701,
    2745,  2789,  2833,  2876,  2920,  2964,  3007,  3051,  3095,  3138,  3182,  3226,  3269,  3313,  3357,  3401,  3444,  3488,  3532,
    3575,  3619,  3663,  3706,  3750,  3794,  3837,  3881,  3925,  3969,  4012,  4056,  4100,  4143,  4187,  4231,  4274,  4318,  4362,
    4405,  4449,  4493,  4537,  4580,  4624,  4668,  4711,  4755,  4799,  4842,  4886,  4930,  4973,  5017,  5061,  5104,  5148,  5192,
    5236,  5279,  5323,  5367,  5410,  5454,  5498,  5541,  5585,  5629,  5672,  5716,  5760,  5804,  5847,  5891,  5935,  5978,  6022,
    6066,  6109,  6153,  6197,  6240,  6284,  6328,  6372,  6415,  6459,  6503,  6546,  6590,  6634,  6677,  6721,  6765,  6808,  6852,
    6896,  6939,  6983,  7027,  7071,  7114,  7158,  7202,  7245,  7289,  7333,  7376,  7420,  7464,  7507,  7551,  7595,  7639,  7682,
    7726,  7770,  7813,  7857,  7901,  7944,  7988,  8032,  8075,  8119,  8163,  8207,  8250,  8294,  8338,  8381,  8425,  8469,  8512,
    8556,  8600,  8643,  8687,  8731,  8774,  8818,  8862,  8906,  8949,  8993,  9037,  9080,  9124,  9168,  9211,  9255,  9299,  9342,
    9386,  9430,  9474,  9517,  9561,  9605,  9648,  9692,  9736,  9779,  9823,  9867,  9910,  9954,  9998,  10042, 10085, 10129, 10173,
    10216, 10260, 10304, 10347, 10391, 10435, 10478, 10522, 10566, 10610, 10653, 10697, 10741, 10784, 10828, 10872, 10915, 10959, 11003,
    11046, 11090, 11134, 11177, 11221, 11265, 11309, 11352, 11396};

const uint16_t g_overflowOffset[256] = {
    0,     32593L, 32418L, 32243, 32068, 31894, 31719, 31544, 31369, 31195, 31020, 30845, 30670, 30496, 30321, 30146, 29971, 29797, 29622,
    29447, 29272, 29097, 28923, 28748, 28573, 28398, 28224, 28049, 27874, 27699, 27525, 27350, 27175, 27000, 26826, 26651, 26476, 26301,
    26127, 25952, 25777, 25602, 25427, 25253, 25078, 24903, 24728, 24554, 24379, 24204, 24029, 23855, 23680, 23505, 23330, 23156, 22981,
    22806, 22631, 22457, 22282, 22107, 21932, 21757, 21583, 21408, 21233, 21058, 20884, 20709, 20534, 20359, 20185, 20010, 19835, 19660,
    19486, 19311, 19136, 18961, 18786, 18612, 18437, 18262, 18087, 17913, 17738, 17563, 17388, 17214, 17039, 16864, 16689, 16515, 16340,
    16165, 15990, 15816, 15641, 15466, 15291, 15116, 14942, 14767, 14592, 14417, 14243, 14068, 13893, 13718, 13544, 13369, 13194, 13019,
    12845, 12670, 12495, 12320, 12146, 11971, 11796, 11621, 11446, 11272, 11097, 10922, 10747, 10573, 10398, 10223, 10048, 9874,  9699,
    9524,  9349,  9175,  9000,  8825,  8650,  8475,  8301,  8126,  7951,  7776,  7602,  7427,  7252,  7077,  6903,  6728,  6553,  6378,
    6204,  6029,  5854,  5679,  5505,  5330,  5155,  4980,  4805,  4631,  4456,  4281,  4106,  3932,  3757,  3582,  3407,  3233,  3058,
    2883,  2708,  2534,  2359,  2184,  2009,  1835,  1660,  1485,  1310,  1135,  961,   786,   611,   436,   262,   87,    65449L, 65274L,
    65100L, 64925L, 64750L, 64575L, 64401L, 64226L, 64051L, 63876L, 63701L, 63527L, 63352L, 63177L, 63002L, 62828L, 62653L, 62478L, 62303L, 62129L, 61954L,
    61779L, 61604L, 61430L, 61255L, 61080L, 60905L, 60731L, 60556L, 60381L, 60206L, 60031L, 59857L, 59682L, 59507L, 59332L, 59158L, 58983L, 58808L, 58633L,
    58459L, 58284L, 58109L, 57934L, 57760L, 57585L, 57410L, 57235L, 57061L, 56886L, 56711L, 56536L, 56361L, 56187L, 56012L, 55837L, 55662L, 55488L, 55313L,
    55138L, 54963L, 54789L, 54614L, 54439L, 54264L, 54090L, 53915L, 53740L};

const uint16_t g_overflowStep[256] = {
    0,   1,   2,   4,   5,   6,   8,   9,   10,  12,  13,  15,  16,  17,  19,  20,  21,  23,  24,  25,  27,  28,  30,  31,  32,  34,
    35,  36,  38,  39,  40,  42,  43,  45,  46,  47,  49,  50,  51,  53,  54,  55,  57,  58,  60,  61,  62,  64,  65,  66,  68,  69,
    70,  72,  73,  75,  76,  77,  79,  80,  81,  83,  84,  86,  87,  88,  90,  91,  92,  94,  95,  96,  98,  99,  101, 102, 103, 105,
    106, 107, 109, 110, 111, 113, 114, 116, 117, 118, 120, 121, 122, 124, 125, 126, 128, 129, 131, 132, 133, 135, 136, 137, 139, 140,
    141, 143, 144, 146, 147, 148, 150, 151, 152, 154, 155, 157, 158, 159, 161, 162, 163, 165, 166, 167, 169, 170, 172, 173, 174, 176,
    177, 178, 180, 181, 182, 184, 185, 187, 188, 189, 191, 192, 193, 195, 196, 197, 199, 200, 202, 203, 204, 206, 207, 208, 210, 211,
    212, 214, 215, 217, 218, 219, 221, 222, 223, 225, 226, 228, 229, 230, 232, 233, 234, 236, 237, 238, 240, 241, 243, 244, 245, 247,
    248, 249, 251, 252, 253, 255, 256, 258, 259, 260, 262, 263, 264, 266, 267, 268, 270, 271, 273, 274, 275, 277, 278, 279, 281, 282,
    283, 285, 286, 288, 289, 290, 292, 293, 294, 296, 297, 299, 300, 301, 303, 304, 305, 307, 308, 309, 311, 312, 314, 315, 316, 318,
    319, 320, 322, 323, 324, 326, 327, 329, 330, 331, 333, 334, 335, 337, 338, 339, 341, 342, 344, 345, 346, 348};

const uint16_t g_deltaAngle[SCREEN_WIDTH] = {
    916,  916,  917,  917,  918,  918,  919,  920,  920,  921,  921,  922,  922,  923,  923,  924,  924,  925,  925,  926,  926,  927,
    927,  928,  929,  929,  930,  930,  931,  931,  932,  933,  933,  934,  934,  935,  935,  936,  937,  937,  938,  938,  939,  940,
    940,  941,  941,  942,  943,  943,  944,  944,  945,  946,  946,  947,  948,  948,  949,  949,  950,  951,  951,  952,  953,  953,
    954,  955,  955,  956,  957,  957,  958,  959,  959,  960,  961,  961,  962,  963,  964,  964,  965,  966,  966,  967,  968,  968,
    969,  970,  971,  971,  972,  973,  973,  974,  975,  976,  976,  977,  978,  979,  979,  980,  981,  982,  982,  983,  984,  985,
    985,  986,  987,  988,  988,  989,  990,  991,  991,  992,  993,  994,  994,  995,  996,  997,  998,  998,  999,  1000, 1001, 1001,
    1002, 1003, 1004, 1005, 1005, 1006, 1007, 1008, 1009, 1009, 1010, 1011, 1012, 1013, 1013, 1014, 1015, 1016, 1017, 1017, 1018, 1019,
    1020, 1021, 1021, 1022, 1023, 0,    0,    0,    1,    2,    3,    3,    4,    5,    6,    7,    7,    8,    9,    10,   11,   11,
    12,   13,   14,   15,   15,   16,   17,   18,   19,   19,   20,   21,   22,   23,   23,   24,   25,   26,   26,   27,   28,   29,
    30,   30,   31,   32,   33,   33,   34,   35,   36,   36,   37,   38,   39,   39,   40,   41,   42,   42,   43,   44,   45,   45,
    46,   47,   48,   48,   49,   50,   51,   51,   52,   53,   53,   54,   55,   56,   56,   57,   58,   58,   59,   60,   60,   61,
    62,   63,   63,   64,   65,   65,   66,   67,   67,   68,   69,   69,   70,   71,   71,   72,   73,   73,   74,   75,   75,   76,
    76,   77,   78,   78,   79,   80,   80,   81,   81,   82,   83,   83,   84,   84,   85,   86,   86,   87,   87,   88,   89,   89,
    90,   90,   91,   91,   92,   93,   93,   94,   94,   95,   95,   96,   97,   97,   98,   98,   99,   99,   100,  100,  101,  101,
    102,  102,  103,  103,  104,  104,  105,  106,  106,  107,  107,  108};

