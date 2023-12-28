#pragma once

/*
 * This is free and unencumbered software released into the public domain.
 *
 * For more information, please refer to <https://unlicense.org>
 */

// Regular text colors
#define BLK "\e[0;30m"  // Black
#define RED "\e[0;31m"  // Red
#define GRN "\e[0;32m"  // Green
#define YEL "\e[0;33m"  // Yellow
#define BLU "\e[0;34m"  // Blue
#define MAG "\e[0;35m"  // Magenta
#define CYN "\e[0;36m"  // Cyan
#define WHT "\e[0;37m"  // White

// Bold text colors
#define BBLK "\e[1;30m"  // Bold Black
#define BRED "\e[1;31m"  // Bold Red
#define BGRN "\e[1;32m"  // Bold Green
#define BYEL "\e[1;33m"  // Bold Yellow
#define BBLU "\e[1;34m"  // Bold Blue
#define BMAG "\e[1;35m"  // Bold Magenta
#define BCYN "\e[1;36m"  // Bold Cyan
#define BWHT "\e[1;37m"  // Bold White

// Underlined text colors
#define UBLK "\e[4;30m"  // Underline Black
#define URED "\e[4;31m"  // Underline Red
#define UGRN "\e[4;32m"  // Underline Green
#define UYEL "\e[4;33m"  // Underline Yellow
#define UBLU "\e[4;34m"  // Underline Blue
#define UMAG "\e[4;35m"  // Underline Magenta
#define UCYN "\e[4;36m"  // Underline Cyan
#define UWHT "\e[4;37m"  // Underline White

// Background colors
#define BLKB "\e[40m"    // Black background
#define REDB "\e[41m"    // Red background
#define GRNB "\e[42m"    // Green background
#define YELB "\e[43m"    // Yellow background
#define BLUB "\e[44m"    // Blue background
#define MAGB "\e[45m"    // Magenta background
#define CYNB "\e[46m"    // Cyan background
#define WHTB "\e[47m"    // White background

// High intensity background colors
#define BLKHB "\e[0;100m" // High intensity black background
#define REDHB "\e[0;101m" // High intensity red background
#define GRNHB "\e[0;102m" // High intensity green background
#define YELHB "\e[0;103m" // High intensity yellow background
#define BLUHB "\e[0;104m" // High intensity blue background
#define MAGHB "\e[0;105m" // High intensity magenta background
#define CYNHB "\e[0;106m" // High intensity cyan background
#define WHTHB "\e[0;107m" // High intensity white background

// High intensity text colors
#define HBLK "\e[0;90m"   // High intensity black
#define HRED "\e[0;91m"   // High intensity red
#define HGRN "\e[0;92m"   // High intensity green
#define HYEL "\e[0;93m"   // High intensity yellow
#define HBLU "\e[0;94m"   // High intensity blue
#define HMAG "\e[0;95m"   // High intensity magenta
#define HCYN "\e[0;96m"   // High intensity cyan
#define HWHT "\e[0;97m"   // High intensity white

// Bold high intensity text colors
#define BHBLK "\e[1;90m"  // Bold high intensity black
#define BHRED "\e[1;91m"  // Bold high intensity red
#define BHGRN "\e[1;92m"  // Bold high intensity green
#define BHYEL "\e[1;93m"  // Bold high intensity yellow
#define BHBLU "\e[1;94m"  // Bold high intensity blue
#define BHMAG "\e[1;95m"  // Bold high intensity magenta

//Reset
#define reset "\e[0m"
#define CRESET "\e[0m"
#define COLOR_RESET "\e[0m"