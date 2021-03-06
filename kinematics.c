#include "kinematics.h"

/** \file G-code axis system to stepper motor axis system conversion.
*/

#include <stdlib.h>

#include "maths.h"


void
carthesian_to_carthesian(const TARGET *startpoint, const TARGET *target,
                         axes_uint32_t delta_um, axes_int32_t steps) {
  uint8_t i;

#ifdef STEPS_PER_M_Z
  for (i = X; i <= Z; i++) {
#else
  for (i = X; i <= Y; i++) {
#endif
    delta_um[i] = (uint32_t)labs(target->axis[i] - startpoint->axis[i]);
    steps[i] = um_to_steps(target->axis[i], i);
  }

  /* Replacing the above five lines with this costs about 200 bytes binary
     size on AVR, but also takes about 120 clock cycles less during movement
     preparation. The smaller version was kept for our Arduino Nano friends.
  delta_um[X] = (uint32_t)labs(target->axis[X] - startpoint->axis[X]);
  steps[X] = um_to_steps(target->axis[X], X);
  delta_um[Y] = (uint32_t)labs(target->axis[Y] - startpoint->axis[Y]);
  steps[Y] = um_to_steps(target->axis[Y], Y);
  delta_um[Z] = (uint32_t)labs(target->axis[Z] - startpoint->axis[Z]);
  steps[Z] = um_to_steps(target->axis[Z], Z);
  */
}

void
carthesian_to_corexy(const TARGET *startpoint, const TARGET *target,
                     axes_uint32_t delta_um, axes_int32_t steps) {

  delta_um[X] = (uint32_t)labs((target->axis[X] - startpoint->axis[X]) +
                               (target->axis[Y] - startpoint->axis[Y]));
  delta_um[Y] = (uint32_t)labs((target->axis[X] - startpoint->axis[X]) -
                               (target->axis[Y] - startpoint->axis[Y]));
#ifdef STEPS_PER_M_Z
  delta_um[Z] = (uint32_t)labs(target->axis[Z] - startpoint->axis[Z]);
#endif
  axes_um_to_steps_corexy(target->axis, steps);
}

void axes_um_to_steps_cartesian(const axes_int32_t um, axes_int32_t steps) {
  uint8_t i;
#ifdef STEPS_PER_M_Z
  for (i = X; i <= Z; i++) {
#else
  for (i = X; i <= Y; i++) {
#endif
    steps[i] = um_to_steps(um[i], i);
  }
}

void axes_um_to_steps_corexy(const axes_int32_t um, axes_int32_t steps) {
  steps[X] = um_to_steps(um[X] + um[Y], X);
  steps[Y] = um_to_steps(um[X] - um[Y], Y);
#ifdef STEPS_PER_M_Z
  steps[Z] = um_to_steps(um[Z], Z);
#endif
}
 
