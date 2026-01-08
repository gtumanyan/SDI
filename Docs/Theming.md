# Theming reference

## Constants
Documentation of constants can be found in the source: https://sourceforge.net/p/snappy-driver-installer-origin/code/HEAD/tree/trunk/source/themelist.h

### Anchor constants
The anchor values are based on four bits: CV, CH, AY, and AX  
CCAA - Center vertical, Center horizontal, Anchor vertical, Anchor horizontal  
VHYX - Y/vertical: 0 = top, 1 = bottom	X/horizontal: 0 = left, 1 = right

| Name            | Value | Binary |
|-----------------|------:|-------:|
| `top_left`      |     0 | `0000` |
| `top_right`     |     1 | `0001` |
| `bottom_left`   |     2 | `0010` |
| `bottom_right`  |     3 | `0011` |
| `center_top`    |     4 | `0100` |
| `center_bottom` |     6 | `0110` |
| `center_left`   |     8 | `1000` |
| `center_right`  |     9 | `1001` |
| `center`        |    12 | `11**` |

### Fill constants
The fill values are based on five bits: AR, FV, FH, TV, and TH  
RA - Keep aspect ratio
FFTT - Fit vertical, Fit horizontal, Tile vertical, Tile horizontal  
VHVH - vertical: 0 = top, 1 = bottom	horizontal: 0 = left, 1 = right

| Name          | Value |  Binary | Description                                                                                         |
|---------------|------:|--------:|-----------------------------------------------------------------------------------------------------|
| `none`        |     0 | `00000` | original dimensions                                                                                 |
| `htile`       |     1 | `00001` | tile horizontally                                                                                   |
| `vtile`       |     2 | `00010` | tile vertically                                                                                     |
| `hstr`        |     4 | `00100` | stretch horizontally to fit the area                                                                |
| `vstr`        |     8 | `01000` | stretch vertically to fit the area                                                                  |
| `hstra`       |    20 | `10100` | stretch horizontally to fit the area while keeping aspect ratio                                     |
| `vstra`       |    24 | `11000` | stretch vertically to fit the area while keeping aspect ratio                                       |
| `htile_vtile` |     3 | `00011` | tile the entire area (htile + vtile)                                                                |
| `vtile_hstr`  |     6 | `00110` | stretch horizontally to fit the area and tile vertically (hstr + vtile)                             |
| `htile_vstr`  |     9 | `01001` | stretch vertically to fit the area and tile horizontally (htile + vstr)                             |
| `hstr_vstr`   |    12 | `01100` | stretch to fill the entire area (hstr + vstr)                                                       |
| `vtile_hstra` |    22 | `11001` | stretch horizontally to fit the area and tile vertically while keeping aspect ratio (vtile + hstra) |
| `htile_vstra` |    25 | `11001` | stretch vertically to fit the area and tile horizontally while keeping aspect ratio (htile + vstra) |
| *missing*     |    28 | `11100` | stretch to fill the entire area while keeping aspect ratio (hstra + vstra)                          |


## Colors
Colors are hex strings in the format `0xAABBGGRR` where `AA` is optional transparency triggered when `AA >= 0x7E`

| Color       | Code       |
|-------------|------------|
| white       | `0xFFFFFF` |
| grey, light | `0xE6E6E6` |
| grey, dark  | `0x4B443B` |
| black       | `0x000000` |


## Driver list items
Driver list items are distinguished by their abbreviation codes

| Code | Type    | Meaning                               | Message                                                          |
|------|---------|---------------------------------------|------------------------------------------------------------------|
| IF   | Info    | Info (indexing,snapshot,installation) |                                                                  |
| IU   | Warn    | No updates                            |                                                                  |
| VR   | Error   | Virus alert                           |                                                                  |
| DO   | Info    | Driver installation                   |                                                                  |
| D1   | Success | Driver installed                      |                                                                  |
| D2   | Warn    | Driver installed (reboot required)    |                                                                  |
| DE   | Error   | Driver installation error             |                                                                  |
| PN   |         | Packname                              |                                                                  |
| SC   | Info    | SAME_CUR                              | Already installed                                                |
| DP   | Info    | DUP                                   | (duplicate)                                                      |
| BN   | Success | BETTER_NEW                            | Updated driver available which is also more optimal              |
| SN   | Success | SAME_NEW                              | Updated driver available                                         |
| BC   | Success | BETTER_CUR                            | More optimal driver available                                    |
| BO   | Success | BETTER_OLD                            | More optimal driver available, though it's older                 |
| WN   | Warn    | WORSE_NEW                             | Less than optimal driver available, however it's more recent     |
| MS   | Warn    | MISSING                               | Driver available (driver not yet installed)                      |
| WC   | Error   | WORSE_CUR                             | Less than optimal driver                                         |
| SO   | Error   | SAME_OLD                              | Old driver                                                       |
| WO   | Error   | WORSE_OLD                             | Old driver and also less than optimal                            |
| IN   | Error   | INVALID                               | Incompatible driver                                              |
| NM   | Special | NOT-FOUND,MISSING                     | Device require a driver but it wasn't found in driverpacks       |
| NU   | Special | NOT-FOUND,INSTELLED_UNKNOWN           | Device is working properly but no driver is found in driverpacks |
| NS   | Special | NOT-FOUND,INSTALLED_STANDARD          | Standard driver                                                  |
