/* The scheduling functions:
 * * receive card's ScheduleInfo and review result from reviewer
 * * calculate new ScheduleInfo and due date */

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "ccq.h"

float get_d_init(int G)
{
    float d_init = w[4] - exp(w[5] * G) + 1;
    return d_init;
}

float get_new_interval(float S)
{
    float new_interval = (S / FACTOR) * (pow(0.9, (1 / DECAY)) - 1);
    return new_interval;
}

float get_mean_reversion(float a, float b)
{
    float mean_reversion = w[7] * a + (1 - w[7]) * b;
    return mean_reversion;
}

float get_sterm_S(float S, int G)
{
    float sterm_S = S * exp(w[17] * (G - 2 + w[18]));
    return sterm_S;
}

float get_R(int t, float S)
{
    float R = pow((1 + FACTOR * t / S), DECAY);
    return R;
}

float get_forget_S(float D, float S, float R)
{
    float forget_S = w[11] * pow(D, -w[12]) *
        (pow((S + 1), w[13]) - 1) * exp(w[14] * (1 - R));
    return forget_S;
}

float get_recall_S(float D, float S, float R)
{
    float recall_S = S * (1 + exp(w[8]) * (11 - D) *
                          pow(S, -w[9]) * (exp(w[10] * (1 - R)) - 1));
    return recall_S;
}

float get_D(float D, int G)
{
    float a = get_d_init(4);
    float b = D - w[6] * (G - 3);
    float D_new = get_mean_reversion(a, b);
    return D_new;
}

ScheduleInfo schedule_card
(ScheduleInfo old_sch, int result);
{
    ScheduleInfo scheduled_card = {0, 0.0, 0.0, 0.0, 0, 0};
    int G = result == '\n' ? 2 : 0; // only grades 0 ("again") and 2 ("good")
    int now = (int)time(NULL);
    float days_since = last != 0 ? (now - last) / 86400 : 0; // 0 if new
    float finterval = 0;

    scheduled_card.last = now;

    if (state == 0) { // new
        scheduled_card.state = 1;
        scheduled_card.D = get_d_init(G);
        scheduled_card.S = w[G];
        scheduled_card.R = 1.0;
        scheduled_card.due = now + 86400;
    } else if (state == 1 && result != '\n') { // young failed
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R); // _recall not _forget bc young
        scheduled_card.R = get_R(days_since, old_sch.S);
        scheduled_card.due = now + 86400;
    } else if (state == 1 && result == '\n') { // young pass
        scheduled_card.state = 2;
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    } else if (state == 2 && result != '\n') { // mature failed
        scheduled_card.state = 1;
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_forget_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        scheduled_card.due = now + 86400;
    } else if (state == 2 && result == '\n') { // mature pass
        scheduled_card.D = get_D(old_sch.D, G);
        scheduled_card.S = get_recall_S(old_sch.D, old_sch.S, old_sch.R);
        scheduled_card.R = get_R(days_since, old_sch.S);
        finterval = get_new_interval(scheduled_card.S);
        scheduled_card.due = now + round(finterval);
    }

    return scheduled_card;
}
