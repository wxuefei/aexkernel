#pragma once

#include <stdio.h>

#define COLOR0 94
#define COLOR1 97

const char header[] = "\
 |##### |#### \\#  /#     |# \n\
 |#   # |#     \\#/#    |### \n\
 |##### |####   \\#       |# \n\
 |#   # |#     /#\\#      |# \n\
 |#   # |#### /#  \\#     |# \n\
";

void init_print_header() {

    char color = COLOR0;
	tty_set_color_ansi(COLOR0);

    char c;
    for (size_t i = 0; i < sizeof(header); i++) {
        c = header[i];

        switch (c) {
            case '\\':
            case '|':
            case '/':
                if (color != COLOR0) {
	                tty_set_color_ansi(COLOR0);
                    color = COLOR0;
                }
                break;
            default:
                if (color != COLOR1) {
	                tty_set_color_ansi(COLOR1);
                    color = COLOR1;
                }
                break;
        }
        putchar(c);
    }
}