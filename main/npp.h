#ifndef _NPP_H
#define _NPP_H

void npp_task_start(void);

void npp_send_button_press(void);
void npp_send_voltage(int voltage_mv);
bool npp_connected();

#endif
