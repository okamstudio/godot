// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Cost tables for level and modes
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/enc/cost_enc.h"

//------------------------------------------------------------------------------
// Level cost tables

// For each given level, the following table gives the pattern of contexts to
// use for coding it (in [][0]) as well as the bit value to use for each
// context (in [][1]).
static const uint16_t VP8LevelCodes[MAX_VARIABLE_LEVEL][2] = {
                  {0x001, 0x000}, {0x007, 0x001}, {0x00f, 0x005},
  {0x00f, 0x00d}, {0x033, 0x003}, {0x033, 0x003}, {0x033, 0x023},
  {0x033, 0x023}, {0x033, 0x023}, {0x033, 0x023}, {0x0d3, 0x013},
  {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013},
  {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x013}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093},
  {0x0d3, 0x093}, {0x0d3, 0x093}, {0x0d3, 0x093}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053},
  {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x053}, {0x153, 0x153}
};

static int VariableLevelCost(int level, const uint8_t probas[NUM_PROBAS]) {
  int pattern = VP8LevelCodes[level - 1][0];
  int bits = VP8LevelCodes[level - 1][1];
  int cost = 0;
  int i;
  for (i = 2; pattern; ++i) {
    if (pattern & 1) {
      cost += VP8BitCost(bits & 1, probas[i]);
    }
    bits >>= 1;
    pattern >>= 1;
  }
  return cost;
}

//------------------------------------------------------------------------------
// Pre-calc level costs once for all

void VP8CalculateLevelCosts(VP8EncProba* const proba) {
  int ctype, band, ctx;

  if (!proba->dirty_) return;  // nothing to do.

  for (ctype = 0; ctype < NUM_TYPES; ++ctype) {
    int n;
    for (band = 0; band < NUM_BANDS; ++band) {
      for (ctx = 0; ctx < NUM_CTX; ++ctx) {
        const uint8_t* const p = proba->coeffs_[ctype][band][ctx];
        uint16_t* const table = proba->level_cost_[ctype][band][ctx];
        const int cost0 = (ctx > 0) ? VP8BitCost(1, p[0]) : 0;
        const int cost_base = VP8BitCost(1, p[1]) + cost0;
        int v;
        table[0] = VP8BitCost(0, p[1]) + cost0;
        for (v = 1; v <= MAX_VARIABLE_LEVEL; ++v) {
          table[v] = cost_base + VariableLevelCost(v, p);
        }
        // Starting at level 67 and up, the variable part of the cost is
        // actually constant.
      }
    }
    for (n = 0; n < 16; ++n) {    // replicate bands. We don't need to sentinel.
      for (ctx = 0; ctx < NUM_CTX; ++ctx) {
        proba->remapped_costs_[ctype][n][ctx] =
            proba->level_cost_[ctype][VP8EncBands[n]][ctx];
      }
    }
  }
  proba->dirty_ = 0;
}

//------------------------------------------------------------------------------
// Mode cost tables.

// These are the fixed probabilities (in the coding trees) turned into bit-cost
// by calling VP8BitCost().
const uint16_t VP8FixedCostsUV[4] = { 302, 984, 439, 642 };
// note: these values include the fixed VP8BitCost(1, 145) mode selection cost.
const uint16_t VP8FixedCostsI16[4] = { 663, 919, 872, 919 };
const uint16_t VP8FixedCostsI4[NUM_BMODES][NUM_BMODES][NUM_BMODES] = {
  { {   40, 1151, 1723, 1874, 2103, 2019, 1628, 1777, 2226, 2137 },
    {  192,  469, 1296, 1308, 1849, 1794, 1781, 1703, 1713, 1522 },
    {  142,  910,  762, 1684, 1849, 1576, 1460, 1305, 1801, 1657 },
    {  559,  641, 1370,  421, 1182, 1569, 1612, 1725,  863, 1007 },
    {  299, 1059, 1256, 1108,  636, 1068, 1581, 1883,  869, 1142 },
    {  277, 1111,  707, 1362, 1089,  672, 1603, 1541, 1545, 1291 },
    {  214,  781, 1609, 1303, 1632, 2229,  726, 1560, 1713,  918 },
    {  152, 1037, 1046, 1759, 1983, 2174, 1358,  742, 1740, 1390 },
    {  512, 1046, 1420,  753,  752, 1297, 1486, 1613,  460, 1207 },
    {  424,  827, 1362,  719, 1462, 1202, 1199, 1476, 1199,  538 } },
  { {  240,  402, 1134, 1491, 1659, 1505, 1517, 1555, 1979, 2099 },
    {  467,  242,  960, 1232, 1714, 1620, 1834, 1570, 1676, 1391 },
    {  500,  455,  463, 1507, 1699, 1282, 1564,  982, 2114, 2114 },
    {  672,  643, 1372,  331, 1589, 1667, 1453, 1938,  996,  876 },
    {  458,  783, 1037,  911,  738,  968, 1165, 1518,  859, 1033 },
    {  504,  815,  504, 1139, 1219,  719, 1506, 1085, 1268, 1268 },
    {  333,  630, 1445, 1239, 1883, 3672,  799, 1548, 1865,  598 },
    {  399,  644,  746, 1342, 1856, 1350, 1493,  613, 1855, 1015 },
    {  622,  749, 1205,  608, 1066, 1408, 1290, 1406,  546,  971 },
    {  500,  753, 1041,  668, 1230, 1617, 1297, 1425, 1383,  523 } },
  { {  394,  553,  523, 1502, 1536,  981, 1608, 1142, 1666, 2181 },
    {  655,  430,  375, 1411, 1861, 1220, 1677, 1135, 1978, 1553 },
    {  690,  640,  245, 1954, 2070, 1194, 1528,  982, 1972, 2232 },
    {  559,  834,  741,  867, 1131,  980, 1225,  852, 1092,  784 },
    {  690,  875,  516,  959,  673,  894, 1056, 1190, 1528, 1126 },
    {  740,  951,  384, 1277, 1177,  492, 1579, 1155, 1846, 1513 },
    {  323,  775, 1062, 1776, 3062, 1274,  813, 1188, 1372,  655 },
    {  488,  971,  484, 1767, 1515, 1775, 1115,  503, 1539, 1461 },
    {  740, 1006,  998,  709,  851, 1230, 1337,  788,  741,  721 },
    {  522, 1073,  573, 1045, 1346,  887, 1046, 1146, 1203,  697 } },
  { {  105,  864, 1442, 1009, 1934, 1840, 1519, 1920, 1673, 1579 },
    {  534,  305, 1193,  683, 1388, 2164, 1802, 1894, 1264, 1170 },
    {  305,  518,  877, 1108, 1426, 3215, 1425, 1064, 1320, 1242 },
    {  683,  732, 1927,  257, 1493, 2048, 1858, 1552, 1055,  947 },
    {  394,  814, 1024,  660,  959, 1556, 1282, 1289,  893, 1047 },
    {  528,  615,  996,  940, 1201,  635, 1094, 2515,  803, 1358 },
    {  347,  614, 1609, 1187, 3133, 1345, 1007, 1339, 1017,  667 },
    {  218,  740,  878, 1605, 3650, 3650, 1345,  758, 1357, 1617 },
    {  672,  750, 1541,  558, 1257, 1599, 1870, 2135,  402, 1087 },
    {  592,  684, 1161,  430, 1092, 1497, 1475, 1489, 1095,  822 } },
  { {  228, 1056, 1059, 1368,  752,  982, 1512, 1518,  987, 1782 },
    {  494,  514,  818,  942,  965,  892, 1610, 1356, 1048, 1363 },
    {  512,  648,  591, 1042,  761,  991, 1196, 1454, 1309, 1463 },
    {  683,  749, 1043,  676,  841, 1396, 1133, 1138,  654,  939 },
    {  622, 1101, 1126,  994,  361, 1077, 1203, 1318,  877, 1219 },
    {  631, 1068,  857, 1650,  651,  477, 1650, 1419,  828, 1170 },
    {  555,  727, 1068, 1335, 3127, 1339,  820, 1331, 1077,  429 },
    {  504,  879,  624, 1398,  889,  889, 1392,  808,  891, 1406 },
    {  683, 1602, 1289,  977,  578,  983, 1280, 1708,  406, 1122 },
    {  399,  865, 1433, 1070, 1072,  764,  968, 1477, 1223,  678 } },
  { {  333,  760,  935, 1638, 1010,  529, 1646, 1410, 1472, 2219 },
    {  512,  494,  750, 1160, 1215,  610, 1870, 1868, 1628, 1169 },
    {  572,  646,  492, 1934, 1208,  603, 1580, 1099, 1398, 1995 },
    {  786,  789,  942,  581, 1018,  951, 1599, 1207,  731,  768 },
    {  690, 1015,  672, 1078,  582,  504, 1693, 1438, 1108, 2897 },
    {  768, 1267,  571, 2005, 1243,  244, 2881, 1380, 1786, 1453 },
    {  452,  899, 1293,  903, 1311, 3100,  465, 1311, 1319,  813 },
    {  394,  927,  942, 1103, 1358, 1104,  946,  593, 1363, 1109 },
    {  559, 1005, 1007, 1016,  658, 1173, 1021, 1164,  623, 1028 },
    {  564,  796,  632, 1005, 1014,  863, 2316, 1268,  938,  764 } },
  { {  266,  606, 1098, 1228, 1497, 1243,  948, 1030, 1734, 1461 },
    {  366,  585,  901, 1060, 1407, 1247,  876, 1134, 1620, 1054 },
    {  452,  565,  542, 1729, 1479, 1479, 1016,  886, 2938, 1150 },
    {  555, 1088, 1533,  950, 1354,  895,  834, 1019, 1021,  496 },
    {  704,  815, 1193,  971,  973,  640, 1217, 2214,  832,  578 },
    {  672, 1245,  579,  871,  875,  774,  872, 1273, 1027,  949 },
    {  296, 1134, 2050, 1784, 1636, 3425,  442, 1550, 2076,  722 },
    {  342,  982, 1259, 1846, 1848, 1848,  622,  568, 1847, 1052 },
    {  555, 1064, 1304,  828,  746, 1343, 1075, 1329, 1078,  494 },
    {  288, 1167, 1285, 1174, 1639, 1639,  833, 2254, 1304,  509 } },
  { {  342,  719,  767, 1866, 1757, 1270, 1246,  550, 1746, 2151 },
    {  483,  653,  694, 1509, 1459, 1410, 1218,  507, 1914, 1266 },
    {  488,  757,  447, 2979, 1813, 1268, 1654,  539, 1849, 2109 },
    {  522, 1097, 1085,  851, 1365, 1111,  851,  901,  961,  605 },
    {  709,  716,  841,  728,  736,  945,  941,  862, 2845, 1057 },
    {  512, 1323,  500, 1336, 1083,  681, 1342,  717, 1604, 1350 },
    {  452, 1155, 1372, 1900, 1501, 3290,  311,  944, 1919,  922 },
    {  403, 1520,  977, 2132, 1733, 3522, 1076,  276, 3335, 1547 },
    {  559, 1374, 1101,  615,  673, 2462,  974,  795,  984,  984 },
    {  547, 1122, 1062,  812, 1410,  951, 1140,  622, 1268,  651 } },
  { {  165,  982, 1235,  938, 1334, 1366, 1659, 1578,  964, 1612 },
    {  592,  422,  925,  847, 1139, 1112, 1387, 2036,  861, 1041 },
    {  403,  837,  732,  770,  941, 1658, 1250,  809, 1407, 1407 },
    {  896,  874, 1071,  381, 1568, 1722, 1437, 2192,  480, 1035 },
    {  640, 1098, 1012, 1032,  684, 1382, 1581, 2106,  416,  865 },
    {  559, 1005,  819,  914,  710,  770, 1418,  920,  838, 1435 },
    {  415, 1258, 1245,  870, 1278, 3067,  770, 1021, 1287,  522 },
    {  406,  990,  601, 1009, 1265, 1265, 1267,  759, 1017, 1277 },
    {  968, 1182, 1329,  788, 1032, 1292, 1705, 1714,  203, 1403 },
    {  732,  877, 1279,  471,  901, 1161, 1545, 1294,  755,  755 } },
  { {  111,  931, 1378, 1185, 1933, 1648, 1148, 1714, 1873, 1307 },
    {  406,  414, 1030, 1023, 1910, 1404, 1313, 1647, 1509,  793 },
    {  342,  640,  575, 1088, 1241, 1349, 1161, 1350, 1756, 1502 },
    {  559,  766, 1185,  357, 1682, 1428, 1329, 1897, 1219,  802 },
    {  473,  909, 1164,  771,  719, 2508, 1427, 1432,  722,  782 },
    {  342,  892,  785, 1145, 1150,  794, 1296, 1550,  973, 1057 },
    {  208, 1036, 1326, 1343, 1606, 3395,  815, 1455, 1618,  712 },
    {  228,  928,  890, 1046, 3499, 1711,  994,  829, 1720, 1318 },
    {  768,  724, 1058,  636,  991, 1075, 1319, 1324,  616,  825 },
    {  305, 1167, 1358,  899, 1587, 1587,  987, 1988, 1332,  501 } }
};

//------------------------------------------------------------------------------
// helper functions for residuals struct VP8Residual.

void VP8InitResidual(int first, int coeff_type,
                     VP8Encoder* const enc, VP8Residual* const res) {
  res->coeff_type = coeff_type;
  res->prob  = enc->proba_.coeffs_[coeff_type];
  res->stats = enc->proba_.stats_[coeff_type];
  res->costs = enc->proba_.remapped_costs_[coeff_type];
  res->first = first;
}

//------------------------------------------------------------------------------
// Mode costs

int VP8GetCostLuma4(VP8EncIterator* const it, const int16_t levels[16]) {
  const int x = (it->i4_ & 3), y = (it->i4_ >> 2);
  VP8Residual res;
  VP8Encoder* const enc = it->enc_;
  int R = 0;
  int ctx;

  VP8InitResidual(0, 3, enc, &res);
  ctx = it->top_nz_[x] + it->left_nz_[y];
  VP8SetResidualCoeffs(levels, &res);
  R += VP8GetResidualCost(ctx, &res);
  return R;
}

int VP8GetCostLuma16(VP8EncIterator* const it, const VP8ModeScore* const rd) {
  VP8Residual res;
  VP8Encoder* const enc = it->enc_;
  int x, y;
  int R = 0;

  VP8IteratorNzToBytes(it);   // re-import the non-zero context

  // DC
  VP8InitResidual(0, 1, enc, &res);
  VP8SetResidualCoeffs(rd->y_dc_levels, &res);
  R += VP8GetResidualCost(it->top_nz_[8] + it->left_nz_[8], &res);

  // AC
  VP8InitResidual(1, 0, enc, &res);
  for (y = 0; y < 4; ++y) {
    for (x = 0; x < 4; ++x) {
      const int ctx = it->top_nz_[x] + it->left_nz_[y];
      VP8SetResidualCoeffs(rd->y_ac_levels[x + y * 4], &res);
      R += VP8GetResidualCost(ctx, &res);
      it->top_nz_[x] = it->left_nz_[y] = (res.last >= 0);
    }
  }
  return R;
}

int VP8GetCostUV(VP8EncIterator* const it, const VP8ModeScore* const rd) {
  VP8Residual res;
  VP8Encoder* const enc = it->enc_;
  int ch, x, y;
  int R = 0;

  VP8IteratorNzToBytes(it);  // re-import the non-zero context

  VP8InitResidual(0, 2, enc, &res);
  for (ch = 0; ch <= 2; ch += 2) {
    for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
        const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
        VP8SetResidualCoeffs(rd->uv_levels[ch * 2 + x + y * 2], &res);
        R += VP8GetResidualCost(ctx, &res);
        it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] = (res.last >= 0);
      }
    }
  }
  return R;
}


//------------------------------------------------------------------------------
// Recording of token probabilities.

// We keep the table-free variant around for reference, in case.
#define USE_LEVEL_CODE_TABLE

// Simulate block coding, but only record statistics.
// Note: no need to record the fixed probas.
int VP8RecordCoeffs(int ctx, const VP8Residual* const res) {
  int n = res->first;
  // should be stats[VP8EncBands[n]], but it's equivalent for n=0 or 1
  proba_t* s = res->stats[n][ctx];
  if (res->last  < 0) {
    VP8RecordStats(0, s + 0);
    return 0;
  }
  while (n <= res->last) {
    int v;
    VP8RecordStats(1, s + 0);  // order of record doesn't matter
    while ((v = res->coeffs[n++]) == 0) {
      VP8RecordStats(0, s + 1);
      s = res->stats[VP8EncBands[n]][0];
    }
    VP8RecordStats(1, s + 1);
    if (!VP8RecordStats(2u < (unsigned int)(v + 1), s + 2)) {  // v = -1 or 1
      s = res->stats[VP8EncBands[n]][1];
    } else {
      v = abs(v);
#if !defined(USE_LEVEL_CODE_TABLE)
      if (!VP8RecordStats(v > 4, s + 3)) {
        if (VP8RecordStats(v != 2, s + 4))
          VP8RecordStats(v == 4, s + 5);
      } else if (!VP8RecordStats(v > 10, s + 6)) {
        VP8RecordStats(v > 6, s + 7);
      } else if (!VP8RecordStats((v >= 3 + (8 << 2)), s + 8)) {
        VP8RecordStats((v >= 3 + (8 << 1)), s + 9);
      } else {
        VP8RecordStats((v >= 3 + (8 << 3)), s + 10);
      }
#else
      if (v > MAX_VARIABLE_LEVEL) {
        v = MAX_VARIABLE_LEVEL;
      }

      {
        const int bits = VP8LevelCodes[v - 1][1];
        int pattern = VP8LevelCodes[v - 1][0];
        int i;
        for (i = 0; (pattern >>= 1) != 0; ++i) {
          const int mask = 2 << i;
          if (pattern & 1) VP8RecordStats(!!(bits & mask), s + 3 + i);
        }
      }
#endif
      s = res->stats[VP8EncBands[n]][2];
    }
  }
  if (n < 16) VP8RecordStats(0, s + 0);
  return 1;
}

//------------------------------------------------------------------------------
