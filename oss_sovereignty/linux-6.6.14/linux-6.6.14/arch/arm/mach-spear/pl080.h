#ifndef __PLAT_PL080_H
#define __PLAT_PL080_H
struct pl08x_channel_data;
int pl080_get_signal(const struct pl08x_channel_data *cd);
void pl080_put_signal(const struct pl08x_channel_data *cd, int signal);
#endif  
