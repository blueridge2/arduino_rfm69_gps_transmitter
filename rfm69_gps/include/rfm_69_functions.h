/* Copyright 2023 by Ralph Blach under the gpl3 public license. see https://www.gnu.org/licenses/gpl-3.0.en.html#license-text
for the entire text*/
#ifndef rfm_69_functions_h
#define rfm_69_functions_h
// function
extern int parse_gps_data(char *const, char **const);
extern void write_gps(const char *, const int);
extern void rfm_69_setup(void);
extern void rfm_69_loop(void);
#endif
