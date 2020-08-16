//
//  unix_input.h
//  LC3
//
//  Created by emp-private-mac-suraj on 16/08/20.
//  Copyright © 2020 Goodpatch. All rights reserved.
//

#ifndef unix_input_h
#define unix_input_h

#include <unistd.h>

uint16_t check_key();
void disable_input_buffering();
void restore_input_buffering();
void handle_interrupt(int signal);

#endif /* unix_input_h */
