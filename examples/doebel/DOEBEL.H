/* Optional PSG reduction of the referenced Evil Jingle opening.
 * Musical attribution and usage: README.md.
 * Generated from editable DOEBEL.json; developer-only generator.
 */
#ifndef DOEBEL_SCORE_H
#define DOEBEL_SCORE_H
#include "lt_score.h"
#define DOEBEL_SERVICE_HZ 240U
#define DOEBEL_END_TICK 2304U
static const lt_score_note doebel_0[] = {
    { 0U, 128U, 320U }, /* M01 F4 */
    { 128U, 192U, 381U }, /* M02 D4 */
    { 192U, 384U, 320U }, /* M03 F4 */
    { 384U, 512U, 269U }, /* M04 G#4 */
    { 512U, 576U, 339U }, /* M05 E4 */
    { 576U, 768U, 269U }, /* M06 G#4 */
    { 768U, 960U, 226U }, /* M07 B4 */
    { 960U, 1088U, 226U }, /* M08 B4 */
    { 1088U, 1280U, 226U }, /* M09 B4 */
    { 1280U, 2112U, 226U }, /* M10 B4 */
};
static const lt_score_note doebel_1[] = {
    { 0U, 128U, 428U }, /* H01 C4 */
    { 192U, 384U, 428U }, /* H02 C4 */
    { 384U, 512U, 339U }, /* H03 E4 */
    { 576U, 768U, 339U }, /* H04 E4 */
    { 768U, 960U, 285U }, /* H05 G4 */
    { 960U, 1088U, 285U }, /* H06 G4 */
    { 1088U, 1280U, 285U }, /* H07 G4 */
    { 1280U, 2112U, 285U }, /* H08 G4 */
};
static const lt_score_note doebel_2[] = {
    { 0U, 192U, 762U }, /* B01 D3 */
    { 192U, 384U, 762U }, /* B02 D3 */
    { 384U, 576U, 679U }, /* B03 E3 */
    { 576U, 768U, 679U }, /* B04 E3 */
    { 768U, 960U, 855U }, /* B05 C3 */
    { 960U, 1088U, 855U }, /* B06 C3 */
    { 1088U, 1280U, 855U }, /* B07 C3 */
    { 1280U, 2112U, 855U }, /* B08 C3 */
};
static const lt_score doebel_score = {
    LTSCORE_VERSION, 4U, 192U, 2112U, 2304U, 355030UL,
    { doebel_0, doebel_1, doebel_2 },
    { 10U, 8U, 8U },
    { 2U, 9U, 7U },
    { 4U, 6U, 8U },
    0, 0U, 0, 0U
};
#endif
