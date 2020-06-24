/*
  #
  # Copyright (c) 2010,2013 LAAS/CNRS
  # All rights reserved.
  #
  # Permission to use, copy, modify, and distribute this software for any purpose
  # with or without   fee is hereby granted, provided   that the above  copyright
  # notice and this permission notice appear in all copies.
  #
  # THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
  # REGARD TO THIS  SOFTWARE INCLUDING ALL  IMPLIED WARRANTIES OF MERCHANTABILITY
  # AND FITNESS. IN NO EVENT SHALL THE AUTHOR  BE LIABLE FOR ANY SPECIAL, DIRECT,
  # INDIRECT, OR CONSEQUENTIAL DAMAGES OR  ANY DAMAGES WHATSOEVER RESULTING  FROM
  # LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
  # OTHER TORTIOUS ACTION,   ARISING OUT OF OR IN    CONNECTION WITH THE USE   OR
  # PERFORMANCE OF THIS SOFTWARE.
  #
  #                                            Xavier BROQUERE on Fri Feb 26 2010
*/
/*--------------------------------------------------------------------
  -- C N R S --
  Laboratoire d'automatique et d'analyse des systemes
  Groupe Robotique et Intelligenxce Artificielle
  7 Avenue du colonel Roche
  31 077 Toulouse Cedex

  Fichier              : softMotion.c
  Fonctions            : Define and Adjust Time Profile of Jerk
  Date de creation     : June 2007
  Date de modification : June 2007
  Auteur               : Xavier BROQUERE

  ----------------------------------------------------------------------*/

#include "softMotionStruct.h"
#include "softMotion.h"
#include "matrixStruct.h"
#include "matrix.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <list>
#include <string>
#include <iostream>
#include <sstream>


#include "time_proto.h"

#include <libxml/xmlreader.h>
#include <time.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#include <cstdlib>
#else
#include <malloc.h>
#endif

#ifndef MY_ALLOC
#define MY_ALLOC(type,n) (type *) malloc((unsigned)(sizeof(type) * n))
#endif

#ifndef MY_FREE
#define MY_FREE(ptr,type,n) free(ptr)
#endif
#define PI 3.14159265
#include "gnuplot_i.hpp"

using std::cout;
using std::cerr;
using std::endl;

static double step_for_bezier = 0.001;

SM_STATUS sm_VerifyInitialAndFinalConditions( SM_LIMITS* limitsGoto, SM_COND* IC, SM_COND* FC, SM_PARTICULAR_VELOCITY* PartVel, SM_COND* ICm, SM_COND* FCm)
{
  double Jmax, Amax, Vmax;
  double A0, V0, Af, Vf;
  double Vsmp, Vsmm, Vfmp, Vfmm;
  double Vs0p, Vs0m, Vf0p, Vf0m;
  double Vtmp, Vlim;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = IC->v;
  A0 = IC->a;
  Vf = FC->v;
  Af = FC->a;

  /* First Verification of Initial and Final Conditions */
  if (Af > Amax)  { Af = Amax;}
  if (Af < -Amax) { Af = -Amax;}
  if (Vf > Vmax)  { Vf = Vmax;}
  if (Vf < -Vmax) { Vf = -Vmax;}
  if (A0 > Amax)  { A0 = Amax;}
  if (A0 < -Amax) { A0 = -Amax;}
  if (V0 > Vmax)  { V0 = Vmax;}
  if (V0 < -Vmax) { V0 = -Vmax;}

  /* Calculs of Particular Velocities */
  Vsmp = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
  Vsmm = V0 - (1/(2*Jmax))*(Amax*Amax - A0*A0);
  Vfmp = Vf + (1/(2*Jmax))*(Amax*Amax - Af*Af);
  Vfmm = Vf - (1/(2*Jmax))*(Amax*Amax - Af*Af);

  Vs0p = V0 - (1/(2*Jmax))*(0 - A0*A0);
  Vs0m = V0 + (1/(2*Jmax))*(0 - A0*A0);
  Vf0p = Vf - (1/(2*Jmax))*(0 - Af*Af);
  Vf0m = Vf + (1/(2*Jmax))*(0 - Af*Af);

  Vtmp = Vmax - (1/(2*Jmax))*(Amax*Amax);
  Vlim = Vtmp - (Vmax - Vtmp);

  /* Second Verification of Initial and Final Conditions */
  if ((Vf > 0) && (Vf0p > Vmax)) {
    //  //printf("CAUTION Final Condition >max: Vf is modified\n");
    Vf0p = Vmax;
    Vf = Vf0p - (1/(2*Jmax))*(Af*Af);
    Vf0m = Vf + (1/(2*Jmax))*(0 - Af*Af);
    Vfmp = Vf + (1/(2*Jmax))*(Amax*Amax - Af*Af);
    Vfmm = Vf - (1/(2*Jmax))*(Amax*Amax - Af*Af);
  }
  if ((Vf < 0) && (Vf0m < -Vmax)) {
    //  //printf("CAUTION Final Condition <min: Vf is modified\n");
    Vf0m = -Vmax;
    Vf = Vf0m + (1/(2*Jmax))*(Af*Af);
    Vf0p = Vf - (1/(2*Jmax))*(0 - Af*Af);
    Vfmp = Vf + (1/(2*Jmax))*(Amax*Amax - Af*Af);
    Vfmm = Vf - (1/(2*Jmax))*(Amax*Amax - Af*Af);
  }
  if ((V0 > 0) && (Vs0p > Vmax)) {
    //   //printf("CAUTION Initial Condition >max: V0 is modified\n");
    Vs0p = Vmax;
    V0 = Vs0p - (1/(2*Jmax))*(A0*A0);
    Vs0m = V0 + (1/(2*Jmax))*(0 - A0*A0);
    Vsmp = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Vsmm = V0 - (1/(2*Jmax))*(Amax*Amax - A0*A0);
  }
  if ((V0 < 0) && (Vs0m < -Vmax)) {
    //  //printf("CAUTION Initial Condition <min: V0 is modified\n");
    Vs0m = -Vmax;
    V0 = Vs0m + (1/(2*Jmax))*(A0*A0);
    Vs0p = V0 - (1/(2*Jmax))*(0 - A0*A0);
    Vsmp = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Vsmm = V0 - (1/(2*Jmax))*(Amax*Amax - A0*A0);
  }

  PartVel->Vsmp = Vsmp;
  PartVel->Vsmm = Vsmm;
  PartVel->Vfmp = Vfmp;
  PartVel->Vfmm = Vfmm;
  PartVel->Vs0p = Vs0p;
  PartVel->Vs0m = Vs0m;
  PartVel->Vf0p = Vf0p;
  PartVel->Vf0m = Vf0m;
  PartVel->Vlim = Vlim;
  ICm->v = V0;
  ICm->a = A0;
  ICm->x = IC->x;
  FCm->v = Vf;
  FCm->a = Af;
  FCm->x = FC->x;
  //  //printf("Conditions initiales et finales OK\n");
  return SM_OK;
}

SM_STATUS sm_CalculOfCriticalLengthLocal( SM_LIMITS* limitsGoto, SM_PARTICULAR_VELOCITY* PartVel, SM_COND* IC, SM_COND* FC, double* dc, int* zone)
{
  double Jmax, Amax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb,Tjp,Tjn,Tac;
  double Vsmp, Vsmm, Vfmp, Vfmm;
  double Vs0p, Vs0m, Vf0p, Vf0m;
  double X1=0, X2=0, A1=0, V1=0;
  double A0, V0, X0, Af, Vf;

  int gg=-1;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;

  /* Init Times */
  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;
  Tjn = 0;
  Tac = 0;
  Tjp = 0;
  /* Set IC and FC */
  V0 = IC->v;
  A0 = IC->a;
  X0 = 0;
  Vf = FC->v;
  Af = FC->a;


  /* Set Particular Velocity */
  Vsmp = PartVel->Vsmp;
  Vsmm = PartVel->Vsmm;
  Vfmp = PartVel->Vfmp;
  Vfmm = PartVel->Vfmm;
  Vs0p = PartVel->Vs0p;
  Vs0m = PartVel->Vs0m;
  Vf0p = PartVel->Vf0p;
  Vf0m = PartVel->Vf0m;

  /* En fonctions des differentes zones : */
  if ((Af==A0) && (Vf ==V0)) {
    //printf("CI et CF identiques\n");
    if (A0>0) {
      A1 = -A0;
      V1 = V0;
      Tjna = (A0-A1) /Jmax;
      X1 = V0*Tjna + (0.5)*A0*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
      *dc = X1 + V1*Tjna + (0.5)*A1*Tjna*Tjna + (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
    }
    if (A0<0) {
      A1 = -A0;
      V1 = V0;
      Tjna = (A1-A0) /Jmax;
      X1 = V0*Tjna + (0.5)*A0*Tjna*Tjna + (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
      *dc = X1 + V1*Tjna + (0.5)*A1*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
    }
    if (A0 == 0) {
      *dc = 0;
    }
    *zone = 0;
  }
  else if ((Af > A0) && (Vs0m == Vf0m)) {
    /* zone 5 */
    *zone = 5;
    if (Af <0 && A0 <0) {
      A1 = -Af;
      V1 = Vf;
      Tjp = (A1 - A0) / Jmax;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp +(1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      Tjn = -(Af - A1)/Jmax;
      *dc = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }
    else if (Af >0 && A0 >0) {
      A1 = -sqrt( Jmax*(V0-Vf) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjn = -(A1 - A0) / Jmax;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn -(1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      Tjp = (Af - A1)/Jmax;
      *dc = X1 + V1*Tjp + (0.5)*A1*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }
    else {
      Tjp = (Af - A0) / Jmax ;
      *dc = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }
  }
  else if ((Af < A0) && (Vs0p == Vf0p)) {
    /* zone 6 */
    *zone = 6;
    if (Af == 0) {
      Tjn = (A0 - Af) / Jmax;
      *dc = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn -(1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }
    else if (Af > 0) {
      A1 = -sqrt( Jmax*(V0-Vf) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjn = -(A1 - A0) / Jmax;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn -(1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      Tjp = (Af - A1)/Jmax;
      *dc = X1 + V1*Tjp + (0.5)*A1*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }
    else if (Af < 0) {
      A1 = sqrt( Jmax*(Vf-V0) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjp = (A1 - A0) / Jmax;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp +(1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      Tjn = -(Af - A1)/Jmax;
      *dc = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }
  }
  else if (Vfmm >= Vsmp) {
    /* zone 1 */
    *zone = 1;

    if ((Vs0p > Vf0m) && Af>0 && A0 >0) {
      A1 = -sqrt( Jmax*(V0 - Vf) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjn = -(A1 - A0) / Jmax;
      Tjp = (Af - A1) / Jmax;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      *dc = X1 + V1*Tjp + (0.5)*A1*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }
    else {

      Tjp = (Amax - A0) / Jmax ;
      Tac = (Vfmm - Vsmp) / Amax ;
      Tjn = (Amax - Af) / Jmax;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      X2 = X1 + Vsmp*Tac + (0.5)*Amax*Tac*Tac;
      *dc = X2 + Vfmm*Tjn + (0.5)*Amax*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }
  }
  else if (Vfmp <= Vsmm) {
    /* zone 2 */
    *zone = 2;

    if ((Vs0m < Vf0p) && (Af < 0) && (A0 <0)) {
      A1 = sqrt( Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjp = (A1 - A0) / Jmax;
      Tjn = -(Af - A1) / Jmax ;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      *dc = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }
    else {
      Tjn = (A0 + Amax) / Jmax;
      Tac = -(Vfmp - Vsmm) / Amax;
      Tjp = (Af + Amax) / Jmax;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn -(1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      X2 = X1 + Vsmm*Tac - (0.5)*Amax*Tac*Tac;
      *dc = X2 + Vfmp*Tjp - (0.5)*Amax*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }

  }
  else if ( ((Vfmp>Vsmp) && (Vfmm>Vsmm)) || ((Af<=0)  && (Vfmm>Vsmm) && (Vfmm<Vsmp) )|| ((Vfmp>Vsmp) && (Af >=A0)) ) {
    /* zone 3 */
    *zone = 3;
    if ((Vs0p > Vf0m) && (Af >0) && (A0 >0)) {
      A1 = -sqrt( Jmax*(V0 - Vf) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjn = -(A1 - A0) / Jmax ;
      Tjp = (Af - A1) / Jmax;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      *dc = X1 + V1*Tjp + (0.5)*A1*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      //printf("j'suis la\n");
    }
    else {
      A1 = sqrt( Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjp = (A1 - A0) / Jmax ;
      Tjn = (A1 - Af) / Jmax;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      *dc = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    }


  }
  else if (((Vfmm<=Vsmm) && (Vfmp>Vsmm)) || ((Af>0) && (Vfmm>Vsmm) && (Vfmp<Vsmp))) {
    /* zone 4 */
    *zone = 4;
    if ((Vs0m < Vf0p) && (Af < 0) && (A0 <0)) {
      A1 = sqrt( Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjp = (A1 - A0) / Jmax;
      Tjn = -(Af - A1) / Jmax ;
      X1 = X0 + V0*Tjp + (0.5)*A0*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
      *dc = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      gg =1 ;
    }
    else {
      gg = 2;
      A1 = - sqrt( Jmax*(V0 - Vf) + (0.5)*(A0*A0 + Af*Af));
      V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
      Tjn = (A0 - A1) / Jmax;
      Tjp = (Af - A1) / Jmax ;
      X1 = X0 + V0*Tjn + (0.5)*A0*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
      *dc = X1 + V1*Tjp + (0.5)*A1*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;
    }

  }
  else {
    *dc = 0;
    printf("pas de zone trouvee\n");

  }

  if (isnan(*dc)) {
    printf("IC.a = %2.8f    IC.v = %2.8f    IC.x = %2.8f\n",IC->a,IC->v,IC->x);
    printf("FC.a = %2.8f    FC.v = %2.8f    FC.x = %f\n",FC->a,FC->v,0.0);
    printf("zone %d\n",*zone);
    printf("A1 %f V1 %f Tjp %f Tjn %f X1 %f dc %f gg %d\n",A1,V1,Tjp,Tjn,X1,*dc,gg);
    printf("V0-Vf %2.8f \n",V0-Vf);

  }

  return SM_OK;
}

SM_STATUS sm_CalculOfDSVmaxType1(SM_LIMITS* limitsGoto, SM_PARTICULAR_VELOCITY* PartVel, SM_COND* ICm, SM_COND* FCm, double* DSVmax)
{
  double Jmax, Amax, Vmax;
  double A0, V0, X0, Af, Vf;
  double Vlim,Vs0m,Vf0m,Vfmp, Vf0p;
  double Tjpa, Taca, Tjna, Tacb, Tjpb,Tjn;
  double A1, A2, A5, A6;
  double V1, V2, V5, V6;
  double X1, X2, X5, X6;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;

  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;

  /* Set Particular Velocities */
  Vfmp = PartVel->Vfmp;
  Vs0m = PartVel->Vs0m;
  Vf0m = PartVel->Vf0m;
  Vlim = PartVel->Vlim;
  Vf0p = PartVel->Vf0p;

  if (Vs0m <= Vlim) {
    A1 = Amax;
    V1 = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;

    V2 = Vmax - (1/(2*Jmax))*(Amax*Amax);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
  }
  else {
    A1 = ABS(sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0) ));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;

    A2 = A1;
    V2 = V1;
    X2 = X1;
  }

  if (Vf0m <= Vlim) {
    A5 = -Amax;
    V5 = Vmax - (1/(2*Jmax))*(Amax*Amax);
    Tjna = - (A5 - A2) / Jmax;
    X5 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
    A6 = -Amax;
    V6 = Vf + (1/(2*Jmax))*(Amax*Amax - Af*Af);
    Tacb = - (V6 - V5) / Amax;
    X6 = X5 + V5*Tacb - (0.5)*Amax*Tacb*Tacb;
    Tjpb = (Af - A6) / Jmax;
    *DSVmax = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  }
  else if (Vf0m > Vlim) {
    if (Vf0p < Vmax) {
      A5 = - sqrt( Jmax*(Vmax-Vf) + (0.5)*(Af*Af) );
      V5 = Vmax - (1/(2*Jmax))*(A5*A5);
    }
    else {
      if (Af < 0) {
	A5 = Af;
	V5 = Vf;
      }
      else {
	A5 = -Af;
	V5 = Vf;
      }
    }
    Tjn = - (A5 - A2) / Jmax;
    X5 = X2 + V2*Tjn + (0.5)*A2*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    A6 = A5;
    V6 = V5;
    X6 = X5;
    Tacb = 0;
    Tjpb = (Af - A6) / Jmax;
    *DSVmax = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  }

  return SM_OK;
}

SM_STATUS sm_CalculOfDSAmaxType1(SM_LIMITS* limitsGoto, SM_PARTICULAR_VELOCITY* PartVel, SM_COND* ICm, SM_COND* FCm, double* DSAmax)
{
  double Jmax, Amax, Vmax;
  double A0, V0, X0, Af, Vf;
  double Vlim,Vs0m,Vf0m,Vfmp, Vsmp,Vs0p, Vf0p;
  double Tjpa, Taca, Tjna, Tacb, Tjpb, Tjn;
  double A1, A2, A5, A6;
  double V1, V2, V5, V6;
  double X1, X2, X5, X6;


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;

  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;

  /* Set Particular Velocities */
  Vfmp = PartVel->Vfmp;
  Vs0m = PartVel->Vs0m;
  Vf0m = PartVel->Vf0m;
  Vlim = PartVel->Vlim;
  Vsmp = PartVel->Vsmp;
  Vs0p = PartVel->Vs0p;
  Vf0p = PartVel->Vf0p;

  if (Vs0m <= Vlim) {
    A1 = Amax;
    V1 = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    if ((Vfmp >= Vsmp) && (Vf0m <= Vlim)) {
      V2 = Vfmp;
    }
    else if ((Vfmp < Vsmp) && (Vs0m <= Vlim)) {
      V2 = Vsmp;
    }
    else {
      V2 = Vmax - (1/(2*Jmax))*Amax*Amax;
    }
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    A2 = A1;
    V2 = V1;
    X2 = X1;
  }
  if (Vf0m <= Vlim) {
    A6 = -Amax;
    V6 = Vf + (1/(2*Jmax))*(Amax*Amax - Af*Af);

    if (V6 <= V2) {
      V5 = V2;
      A5 = -Amax;
    }
    else {
      V5 = V6;
      A5 = -Amax;
    }
    Tjna = - (A5 - A2) / Jmax;
    X5 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
    Tacb = - (V6 - V5) / Amax;
    X6 = X5 + V5*Tacb - (0.5)*Amax*Tacb*Tacb;
    Tjpb = (Af - A6) / Jmax;
    *DSAmax = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  }
  else if (Vf0m > Vlim) {
    if (Vf0p < Vmax) {
      A5 = - sqrt( Jmax*(Vmax-Vf) + (0.5)*(Af*Af) );
      V5 = Vmax - (1/(2*Jmax))*(A5*A5);
    }
    else {
      if (Af < 0) {
	A5 = Af;
	V5 = Vf;
      }
      else {
	A5 = -Af;
	V5 = Vf;
      }
    }
    Tjn = - (A5 - A2) / Jmax;
    X5 = X2 + V2*Tjn + (0.5)*A2*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    A6 = A5;
    V6 = V5;
    X6 = X5;
    Tacb = 0;
    Tjpb = (Af - A6) / Jmax;
    *DSAmax = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  }

  return SM_OK;
}

SM_STATUS sm_VerifyTimesType1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_TIMES* Time, double epsilon_erreur)
{
  double Tjpa, Tjpa2, Tjpa3;
  double Taca, Taca2;
  double Tjna, Tjna2, Tjna3;
  double Tvc;
  double Tjnb, Tjnb2, Tjnb3;
  double Tacb, Tacb2;
  double Tjpb, Tjpb2, Tjpb3;
  double Jmax, Amax, Vmax;
  double A0, V0, X0, Xf, Af, Vf;
  double A1, V1, X1, A2, V2, X2, A3, V3, X3, A4, V4, X4,A5, V5, X5, A6, V6, X6;
  double Af_cc, Vf_cc, Xf_cc;
  double epsilon = epsilon_erreur;
  double epsilon_times = 1e-08;

  //  printf("epsilon_erreur %f\n", epsilon);
  // epsilon = 1e-06;
  /* Set Times */
  Tjpa = Time->Tjpa;
  Taca = Time->Taca;
  Tjna = Time->Tjna;
  Tvc  = Time->Tvc;
  Tjnb = Time->Tjnb;
  Tacb = Time->Tacb;
  Tjpb = Time->Tjpb;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = ICm->x;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa2 = Tjpa*Tjpa;
  Tjpa3 = Tjpa*Tjpa2;
  Taca2 = Taca*Taca;
  Tjna2 = Tjna*Tjna;
  Tjna3 = Tjna*Tjna2;
  Tjnb2 = Tjnb*Tjnb;
  Tjnb3 = Tjnb*Tjnb2;
  Tacb2 = Tacb*Tacb;
  Tjpb2 = Tjpb*Tjpb;
  Tjpb3 = Tjpb*Tjpb2;
  /*   printf("Tjpa %f\n",Tjpa); */
  /*   printf("Taca %f\n",Taca); */
  /*   printf("Tjna %f\n",Tjna); */
  /*   printf("Tvc %f\n",Tvc); */
  /*   printf("Tjnb %f\n",Tjnb); */
  /*   printf("Tacb %f\n",Tacb); */
  /*   printf("Tjpb %f\n",Tjpb); */



  if (ABS(Tjpa) < epsilon_times) { Tjpa = 0;}
  if (ABS(Taca) < epsilon_times) { Taca = 0;}
  if (ABS(Tjna) < epsilon_times) { Tjna = 0;}
  if (ABS(Tvc) < epsilon_times) { Tvc = 0;}
  if (ABS(Tjnb) < epsilon_times) { Tjnb = 0;}
  if (ABS(Tacb) < epsilon_times) { Tacb = 0;}
  if (ABS(Tjpb) < epsilon_times) { Tjpb = 0;}

  if (Tjpa<0) {
    //printf("Times incorrect\n");
    //printf("Tjpa<0\n");

    //  return SM_ERROR;
  }
  if (Taca<0) {
    //printf("Times incorrect\n");
    //printf("Taca<0\n");
    // return SM_ERROR;
  }

  if (Tjna<0) {
    //printf("Times incorrect\n");
    //printf("Tjna<0\n");
    // return SM_ERROR;
  }

  if (Tvc<0) {
    //printf("Times incorrect\n");
    //printf("Tvc<0\n");
    // return SM_ERROR;
  }

  if (Tjnb<0) {
    //printf("Times incorrect\n");
    //printf("Tjnb<0\n");
    //  return SM_ERROR;
  }

  if (Tacb<0) {
    //printf("Times incorrect\n");
    //printf("Tacb<0\n");
    // return SM_ERROR;
  }

  if (Tjpb<0) {
    //printf("Times incorrect\n");
    //printf("Tjpb<0\n");
    //  return SM_ERROR;
  }

  A1 = A0 + Jmax*Tjpa;
  V1 = V0 + A0*Tjpa + Jmax*Tjpa2/2;
  X1 = X0 + V0*Tjpa + A0*Tjpa2/2 + Jmax*Tjpa3/6;
  A2 = A1;
  V2 = V1 + A1*Taca;
  X2 = X1 + V1*Taca + A1*Taca2/2;
  A3 = A2 - Jmax*Tjna;
  V3 = V2 + A2*Tjna - Jmax*Tjna2/2;
  X3 = X2 + V2*Tjna + A2*Tjna2/2 - Jmax*Tjna3/6;
  A4  = A3;
  V4  = V3;
  X4  = X3 + V3*Tvc;
  A5 = A4  -  Jmax*Tjnb;
  V5 = V4  +  A4*Tjnb -  Jmax*Tjnb2/2;
  X5 = X4  +  V4*Tjnb +  A4*Tjnb2/2 - Jmax*Tjnb3/6;
  A6 = A5;
  V6 = V5 + A5*Tacb;
  X6 = X5 + V5*Tacb + A5*Tacb2/2;
  Af_cc = A6 + Jmax*Tjpb;
  Vf_cc = V6 + A6*Tjpb + Jmax*Tjpb2/2;
  Xf_cc = X6 + V6*Tjpb + A6*Tjpb2/2 + Jmax*Tjpb3/6;


  if ((ABS(Xf-Xf_cc)<epsilon) && (ABS(Vf-Vf_cc)<epsilon) && (ABS(Af-Af_cc)<epsilon)) {
    //printf("    ******************************\n");
    //printf("    |     Times are correct      |\n");
    //printf("    ******************************\n");
  }
  else {
    printf("Times incorrect\n");
    printf("Tjpa %f\n",Tjpa);
    printf("Taca %f\n",Taca);
    printf("Tjna %f\n",Tjna);
    printf("Tvc %f\n",Tvc);
    printf("Tjnb %f\n",Tjnb);
    printf("Tacb %f\n",Tacb);
    printf("Tjpb %f\n",Tjpb);
    printf("IC.a %f\n",ICm->a);
    printf("IC.v %f\n",ICm->v);
    printf("IC.x %f\n",ICm->x);
    printf("FC.a %f\n",FCm->a);
    printf("FC.v %f\n",FCm->v);
    printf("FC.x %f\n",FCm->x);
    printf("Erreur Position %f\n", ABS(Xf-Xf_cc));
    printf("Erreur Vitesse %f\n", ABS(Vf-Vf_cc));
    printf("Erreur Acceleration %f\n", ABS(Af-Af_cc)); 
   return SM_ERROR;
  }
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_VmaxAmin_7seg_maxi(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /*Fonction permettant de calculer le profil du jerk pour un chemin type 1
    avec Vmax atteind et compose de 7 segments maximum */

  double Jmax, Amax, Vmax;
  double A1, V1, X1, A2, V2, X2, A3, V3, X3, A4, V4, X4,A5, V5, X5, A6, V6, X6;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;

  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;
  //printf ("Vlim %g", (PartVel->Vlim));
  //printf ("Vs0m %g", (PartVel->Vs0m));


  if ((PartVel->Vs0m) < (PartVel->Vlim)) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    //printf ("Tjpa\n %g", Tjpa);
    V2 = Vmax - (1/(2*Jmax))*(Amax*Amax - 0);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
  }
  else {

    A1 = sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0) );
    V1 = V0 + (1.0/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;

    A2 = A1;
    V2 = V1;
    Taca = 0;
    X2 = X1;
  }



  if ((PartVel->Vs0p) == Vmax  && A0>0) { Tjpa = 0;}

  Tjna = A2 / Jmax;
  A3 = 0;
  V3 = Vmax;
  X3 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
  A4 = 0;
  V4 = Vmax;


  if (PartVel->Vf0p < Vmax) {
    A5 = -Amax;
    Tjnb = - A5 / Jmax;
    V5 = V4 + A4*Tjnb - (0.5)*Jmax*Tjnb*Tjnb;
    A6 = -Amax;
    Tjpb = (Af -(-Amax)) / Jmax;
    V6 = Vf - A6*Tjpb - (0.5)*Jmax*Tjpb*Tjpb;
    X6 = Xf - V6*Tjpb - (0.5)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
    Tacb = - (V6 - V5) / Amax;
    X5 = X6 - V5*Tacb + (0.5)*Amax*Tacb*Tacb;


  }

  else {
    A5 = -Af;
    Tjnb = - A5 / Jmax;
    V5 = V4 + A4*Tjnb - (0.5)*Jmax*Tjnb*Tjnb;
    A6 = A5;
    Tjpb = (Af -A6) / Jmax;
    V6 = V5;
    X6 = Xf - V6*Tjpb - (0.5)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
    Tacb = - (V6 - V5) / Amax;
    X5 = X6 - V5*Tacb + (0.5)*Amax*Tacb*Tacb;
  }

  X4 = X5 -V4*Tjnb - (0.5)*A4*Tjnb*Tjnb + (1.0/6.0)*Jmax*Tjnb*Tjnb*Tjnb;
  Tvc = (X4 - X3) / Vmax;


  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_VmaxAmin_5seg_maxi(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Vmax atteind  et compose de 5 segments maximum */
  double Jmax, Amax, Vmax;
  double A1, V1, X1, A2, V2, X2, A3, V3, X3, A4, V4, X4,A5, V5, X5;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (PartVel->Vs0m < PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    V2 = Vmax - (1/(2*Jmax))*(Amax*Amax - 0);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
    if (Tjpa != 0) {
      // //printf("Chemin a 5 segments Tacb=0\n");
    }
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0));
    V1 = V0 + (1.0/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    A2 = A1;
    V2 = V1;
    Taca = 0;
    X2 = X1;
  }

  if ((Taca != 0) && (Tjpa == 0)) {
    // //printf("Chemin a 4 segments  Tjpa=0 Tacb=0\n");
  }
  else if (Taca == 0 && Tjpa == 0) {
    // //printf("Chemin a 3 segments Tjpa=0 Taca=0 Tacb=0\n");
  }
  Tjna = A2 / Jmax;
  A3 = 0;
  V3 = Vmax;
  X3 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
  A4 = 0;
  V4 = Vmax;
  A5 = Af;
  V5 = Vf;
  X5 = Xf;
  Tjnb = - A5 / Jmax;
  Tjpb = 0;
  Tacb = 0;
  X4 = X5 -V4*Tjnb - (0.5)*A4*Tjnb*Tjnb + (1.0/6.0)*Jmax*Tjnb*Tjnb*Tjnb;
  Tvc = (X4 - X3) / Vmax;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_VmaxAmin_6seg_maxi(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Vmax atteinds  et compose de 6 segments maximum */
  double Jmax, Amax, Vmax;
  double A1, V1, X1, A2=0, V2=0, X2=0, A3, V3, X3, A4, V4, X4,A5, V5, X5;;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (PartVel->Vs0m < PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    V2 = Vmax - (1.0/(2*Jmax))*(Amax*Amax - 0);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
    if (Tjpa != 0) {
      // //printf("Chemin a 6 segments Tacb=0\n");
    }
    else {
      A1 = ABS(sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0) ));
      V1 = V0 + (1.0/(2*Jmax))*(A1*A1 - A0*A0);
      Tjpa = (A1 - A0) / Jmax;
      X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
      A2 = A1;
      V2 = V1;
      Taca = 0;
      X2 = X1;
    }
  }
  if ((Taca != 0) && (Tjpa == 0)) {
    // //printf("Chemin a 5 segments Tjpa=0 Tacb=0\n");
  }
  else if (Taca == 0 && Tjpa == 0) {
    // //printf("Chemin a 4 segments Tjpa=0 Taca=0 Tacb\n");
  }
  Tjna = A2 / Jmax;
  A3 = 0;
  V3 = Vmax;
  X3 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
  A4 = 0;
  V4 = Vmax;
  A5 = -Amax;
  // A5 = - sqrt( Jmax*(V4-Vf) + (0.5)*(Af*Af + A4*A4) );
  Tjnb = - A5 / Jmax;
  V5 = V4 + A4*Tjnb - (0.5)*Jmax*Tjnb*Tjnb;
  //  A6 = A5;
  // Tjpb = (Af - A6) / Jmax;
  //  V6 = Vf - A6*Tjpb - (0.5)*Jmax*Tjpb*Tjpb;
  // X6 = Xf - V6*Tjpb - (0.5)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  Tacb = (V5-Vf)/Amax;
  // X5 = X6;
  X5 = Xf - V5*Tacb - (0.5)*A5*Tacb*Tacb;
  X4 = X5 -V4*Tjnb - (0.5)*A4*Tjnb*Tjnb + (1.0/6.0)*Jmax*Tjnb*Tjnb*Tjnb;
  Tvc = (X4 - X3) / Vmax;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_Vmax_5seg_maxi(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Vmax atteind 5 segments maximun */
  double Jmax, Amax, Vmax;
  double A1, V1, X1, A2=0, V2=0, X2=0, A3, V3, X3, A4, V4, X4,A5, V5, X5;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (PartVel->Vs0m < PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    V2 = Vmax - (1/(2*Jmax))*(Amax*Amax - 0);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
    if (Tjpa != 0) {
      //   //printf("Chemin a 5 segments\n");
    }
  }
  else {


    A1 = ABS(sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0) ));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    A2 = A1;
    V2 = V1;
    Taca = 0;
    X2 = X1;
  }

  if ((Taca != 0) && (Tjpa == 0)) {
    // //printf("Chemin a 4 segments  Tjpa=0 Tacb=0\n");
  }
  else if ((Taca == 0) && (Tjpa == 0)) {
    //  //printf("Chemin a 3 segments Tjpa=0 Taca=0 Tacb=0\n");
  }
  Tjna = A2 / Jmax;
  A3 = 0;
  V3 = Vmax;
  X3 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
  A4 = 0;
  V4 = Vmax;
  A5 = Af;
  V5 = Vf;
  X5 = Xf;
  Tjnb = - A5 / Jmax;
  Tjpb = 0;
  Tacb = 0;
  X4 = X5 -V4*Tjnb - (0.5)*A4*Tjnb*Tjnb + (1.0/6.0)*Jmax*Tjnb*Tjnb*Tjnb;
  Tvc = (X4 - X3) / Vmax;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_Vmax_6seg_maxi(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Vmax atteinds  et compose de 6 segments maximum*/
  double Jmax, Amax, Vmax;
  double A1, V1, X1, A2=0, V2=0, X2=0, A3, V3, X3, A4, V4, X4,A5, V5, X5, A6, V6, X6;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (PartVel->Vs0m < PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    V2 = Vmax - (1.0/(2*Jmax))*(Amax*Amax - 0);
    A2 = Amax;
    Taca = (V2 - V1) / Amax;
    X2 = X1 + V1*Taca + (0.5)*Amax*Taca*Taca;
    if (Tjpa != 0) {
      //  //printf("Chemin a 6 segments Tacb=0\n");
    }
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (0.5)*(A0*A0));
    V1 = V0 + (1.0/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    //printf("Tjpa %g\n",Tjpa);
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    A2 = A1;
    V2 = V1;
    Taca = 0;
    X2 = X1;
  }

  if ((Taca != 0) && (Tjpa == 0)) {
    // //printf("Chemin a 5 segments Tjpa=0 Tacb=0\n");
  }
  else if (Taca == 0 && Tjpa == 0) {
    //  //printf("Chemin a 4 segments Tjpa=0 Taca=0 Tacb\n");
  }

  Tjna = A2 / Jmax;
  A3 = 0;
  V3 = Vmax;
  X3 = X2 + V2*Tjna + (0.5)*A2*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna;
  A4 = 0;
  V4 = Vmax;
  A5 = - sqrt( Jmax*(V4-Vf) + (0.5)*(Af*Af + A4*A4) );
  Tjnb = - A5 / Jmax;
  V5 = V4 + A4*Tjnb - (0.5)*Jmax*Tjnb*Tjnb;
  A6 = A5;
  Tjpb = (Af - A6) / Jmax;
  V6 = Vf - A6*Tjpb - (0.5)*Jmax*Tjpb*Tjpb;
  X6 = Xf - V6*Tjpb - (0.5)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  Tacb = 0;
  X5 = X6;
  X4 = X5 -V4*Tjnb - (0.5)*A4*Tjnb*Tjnb + (1.0/6.0)*Jmax*Tjnb*Tjnb*Tjnb;
  Tvc = (X4 - X3) / Vmax;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_VmaxAmin_4seg(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Amax et Amin atteind et compose de 4 segments*/
  double Jmax, Amax, Vmax;
  double A1, V1, X1, V2=0, V5, A6, V6;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Tjn12, Tjn13, Tjn14, Amax2, Jmax2, Tjpb2, Tjpb3;
  double Tjn1, Taca1, Tacb1, Tadd=-1;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  Tvc = 0;
  A1 = Amax;
  V1 = PartVel->Vsmp;
  Tjpa = (Amax - A0) / Jmax;
  X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  A6 = - Amax;
  V6 = PartVel->Vfmp;
  Tjpb = (Af - A6) / Jmax;
  Tjn1 = 2 * Amax / Jmax;
  Tjn12 = Tjn1*Tjn1;
  Tjn13 = Tjn12 * Tjn1;
  Tjn14 = Tjn13 * Tjn1;
  Amax2 = Amax*Amax;
  Jmax2 = Jmax*Jmax;
  Tjpb2 = Tjpb*Tjpb;
  Tjpb3 = Tjpb2*Tjpb;

  if ( V1 >= V6 ) {
    V5=V1;
    Tacb1 = - ( V6 - V5) / Amax;
    Tadd = -1.0/12.0*(12*V1+12*Amax*Tjn1-3*Jmax*Tjn12-sqrt(144*V1*V1+144*Amax*Tjn1*V1-72*Jmax*Tjn12*V1+72*Amax2*Tjn12-48*Jmax*Tjn13*Amax+9*Jmax2*Tjn14+144*Amax*Xf-144*Amax*X1-24*Amax*Jmax*Tjpb3-144*Amax2*Tjn1*Tacb1+72*Amax*Jmax*Tjn12*Tacb1-144*Amax*V1*Tacb1-144*Amax*V6*Tjpb-72*Amax*A6*Tjpb2+72*Amax2*Tacb1*Tacb1))/Amax;
    Tacb = Tacb1 + Tadd;
    Taca = Tadd;
    Tjna = Tjn1 / 2;
    Tjnb = Tjna;
  }
  else {
    V2 = V6;
    Taca1 = (V2 - V1) / Amax;
    Tadd = -1.0/12.0*(12*Amax*Taca1-3*Jmax*Tjn12+12*Amax*Tjn1+12*V1-sqrt(72*Amax2*Taca1*Taca1-72*Amax*Taca1*Jmax*Tjn12+144*Amax2*Taca1*Tjn1+144*Amax*Taca1*V1+9*Jmax2*Tjn14-48*Jmax*Tjn13*Amax-72*Jmax*Tjn12*V1+72*Amax2*Tjn12+144*Amax*Tjn1*V1+144*V1*V1+144*Amax*Xf-144*Amax*X1-144*Amax*V6*Tjpb-72*Amax*A6*Tjpb2-24*Amax*Jmax*Tjpb3))/Amax;
    Tacb = Tadd;
    Taca = Taca1 + Tadd;
    Tjna = Tjn1/2;
    Tjnb = Tjna;
  }
  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_Z0(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_TIMES* Time)
{
  /*  Fonction permettant de calculer le profil du jerk pour un chemin type 1
      avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 0: IF = IC*/
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Jmax2, Jmax3, Jmax4;
  double A02, A03, A04, V02, V03, Xf2;
  double Tj, Tj01, Tj012, Tj013, Tj014, Tj015, Tj016;
  double Tj02, Tj022, Tj023, Tj024, Tj025, Tj026;
  double A0, V0, X0, Xf, Af, Vf;
  double x0, x1, i=0, cg, A1;


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (A0 >= 0) {
    Tj01 = ABS(A0 / Jmax);
  }
  else {
    Tj01 = -ABS(A0 / Jmax);
  }
  if (Af >= 0) {
    Tj02 = ABS(Af / Jmax);
  }
  else {
    Tj02 = -ABS(Af / Jmax);
  }

  Jmax2 = Jmax*Jmax;
  Jmax3 = Jmax2*Jmax;
  Jmax4 = Jmax3*Jmax;

  Tj012 = Tj01*Tj01;
  Tj013 = Tj012*Tj01;
  Tj014 = Tj013*Tj01;
  Tj015 = Tj014*Tj01;
  Tj016 = Tj015*Tj01;
  Tj022 = Tj02*Tj02;
  Tj023 = Tj022*Tj02;
  Tj024 = Tj023*Tj02;
  Tj025 = Tj024*Tj02;
  Tj026 = Tj025*Tj02;

  A02 = A0*A0;
  A03 = A02*A0;
  A04 = A03*A0;

  V02 = V0*V0;
  V03 = V02*V0;
  Xf2 = Xf*Xf;


  Taca = 0;
  Tvc = 0;
  Tacb = 0;
  x0 =Amax/Jmax;

  for(i=0;i<20;i++) {
    Tj=x0;
    cg = (-Xf + V0 * (Tj - Tj01) + A0 * pow(Tj - Tj01, 0.2e1) / 0.2e1 + Jmax * pow(Tj - Tj01, 0.3e1) / 0.6e1 + 0.2e1 * (V0 + A0 * (Tj - Tj01) + Jmax * pow(Tj - Tj01, 0.2e1) / 0.2e1) * Tj + 0.2e1 * (A0 + Jmax * (Tj - Tj01)) * Tj * Tj - 0.4e1 / 0.3e1 * Jmax * pow(Tj, 0.3e1) + (V0 + A0 * (Tj - Tj01) + Jmax * pow(Tj - Tj01, 0.2e1) / 0.2e1) * (Tj + Tj02) - (A0 + Jmax * (Tj - Tj01)) * pow(Tj + Tj02, 0.2e1) / 0.2e1 + Jmax * pow(Tj + Tj02, 0.3e1) / 0.6e1) / (0.4e1 * V0 + 0.4e1 * A0 * (Tj - Tj01) + 0.2e1 * Jmax * pow(Tj - Tj01, 0.2e1) + 0.2e1 * (A0 + Jmax * (Tj - Tj01)) * Tj - 0.2e1 * Jmax * Tj * Tj + 0.8e1 * (A0 / 0.2e1 + Jmax * (Tj - Tj01) / 0.2e1) * Tj + (A0 + Jmax * (Tj - Tj01)) * (Tj + Tj02) - 0.2e1 * (A0 / 0.2e1 + Jmax * (Tj - Tj01) / 0.2e1) * (Tj + Tj02));

    x1 = x0 - cg;
    x0=x1;
    Tj=x1;
  }

  Tjpa = (Tj) - Tj01;
  Tjpb = (Tj) + Tj02;
  A1 = A0 +Jmax*Tjpa;
  Tjna = Tj;
  Tjnb = Tj;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_Z1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time, double* dc)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 1*/
  double Jmax, Amax, Vmax;
  double A1, V1, X1;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Tjn, Tjn1;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf, V2;

  double epsilon=0.00000001;
  double Tjpb_0;
  double x1, x0, cg;


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;


  A1 = Amax;
  Tjpa = (Amax - A0) / Jmax;
  V1 = V0 + A0*Tjpa + (0.5)*Jmax*Tjpa*Tjpa;
  X1 = V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  Tjn1 = (Amax - Af) / Jmax;



  x0 =2*Amax/Jmax;
  i = 0;
  do {
    i++;
    Tjpb=x0;
    Tjpb_0=Tjpb;
    cg = (-Xf + X1 + V1 * (Vf - 0.1e1 / Jmax * (Amax * Amax - Af * Af) / 0.2e1 - V1) / Amax + 0.1e1 / Amax * pow(Vf - 0.1e1 / Jmax * (Amax * Amax - Af * Af) / 0.2e1 - V1, 0.2e1) / 0.2e1 + (Vf - 0.1e1 / Jmax * (Amax * Amax - Af * Af) / 0.2e1) * Tjn1 + Amax * Tjn1 * Tjn1 / 0.2e1 - Jmax * pow(Tjn1, 0.3e1) / 0.6e1 + (-Jmax * Tjn1 * Tjn1 / 0.2e1 - 0.1e1 / Jmax * Amax * Amax / 0.2e1 - 0.2e1 * Af * Tjn1 - 0.1e1 / Amax * pow(Af, 0.3e1) / Jmax - 0.2e1 / Amax * Vf * Af + Amax * Af / Jmax + 0.2e1 * Vf + 0.1e1 / Jmax * Af * Af / 0.2e1 + Amax * Tjn1) * Tjpb + (0.1e1 / Amax * Vf * Jmax + 0.5e1 / 0.2e1 / Amax * Af * Af + Jmax * Tjn1 / 0.2e1 - 0.5e1 / 0.2e1 * Af) * Tjpb * Tjpb + (Jmax - 0.2e1 / Amax * Af * Jmax) * pow(Tjpb, 0.3e1) + 0.1e1 / Amax * Jmax * Jmax * pow(Tjpb, 0.4e1) / 0.2e1) / (-Jmax * Tjn1 * Tjn1 / 0.2e1 - 0.1e1 / Jmax * Amax * Amax / 0.2e1 - 0.2e1 * Af * Tjn1 - 0.1e1 / Amax * pow(Af, 0.3e1) / Jmax - 0.2e1 / Amax * Vf * Af + Amax * Af / Jmax + 0.2e1 * Vf + 0.1e1 / Jmax * Af * Af / 0.2e1 + Amax * Tjn1 + (0.2e1 / Amax * Vf * Jmax + 0.5e1 / Amax * Af * Af + Jmax * Tjn1 - 0.5e1 * Af) * Tjpb + (0.3e1 * Jmax - 0.6e1 / Amax * Af * Jmax) * Tjpb * Tjpb + 0.2e1 / Amax * Jmax * Jmax * pow(Tjpb, 0.3e1));

    x1 = x0 - cg/4;
    x0=x1;

    if ( x0 <0) { x0 = 0;}
    Tjpb=x1;

    if (i > 150) { break;}

  } while (ABS(Tjpb_0 -Tjpb) > epsilon);
  //printf("Convergence de Newton en %d iterations\n",i);


  V2 = Vf - (Af - Jmax*Tjpb)*Tjpb -(0.5)*Jmax*Tjpb*Tjpb - (1/(2*Jmax))*(Amax*Amax-(Af - Jmax*Tjpb)*(Af - Jmax*Tjpb));

  Taca = (V2-V1)/Amax;
  Tjn = Tjn1 + Tjpb;

  if (Tjn >= Amax/Jmax) {
    Tjna = Amax/Jmax;
    Tjnb = Tjn-Tjna;
  }
  else {
    Tjnb=0;
    Tjna=Tjn;
  }
  Tvc = 0;
  Tacb= 0;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;


  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSZ1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double* DSZ1)
{
  //function [DSZ1]=calcul_Of_DSZ1_Z1(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Xf,Vsmp)
  double Jmax, Amax, Vmax;
  double Tjpa, Tjn, Tjp;
  double X1, A1, V1, A2, V2, X2;
  double A0, V0, Af, Vf;


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Init Times */
  Tjpa = 0;
  Tjn = 0;
  Tjp = 0;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;


  Tjpa = (Amax - A0)/Jmax;
  A1 = Amax;
  V1 = PartVel->Vsmp;
  A2 = sqrt( Jmax*(V1 - Vf) + (0.5)*(A1*A1 + Af*Af));
  V2 = V1 - (1.0/(2*Jmax))*(A2*A2 - A1*A1);
  Tjn = -(A2 - A1) / Jmax;
  Tjp = (Af - A2) / Jmax;

  X1 = V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  X2 = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
  *DSZ1 = X2 + V2*Tjp + (0.5)*A2*Tjp*Tjp + (1.0/6.0)*Jmax*Tjp*Tjp*Tjp;

  return SM_OK;
}


SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_inf_DSZ1_Z1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 1*/
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;


  double Tjndc;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf;
  double Vsmp = PartVel->Vsmp;
  double epsilon=0.00000001;
  double Tj, Tj_0;
  double x1, x0, cg;
  //function [Tjpa,Taca,Tjna,Tvc,Tjnb,Tacb,Tjpb]=sm_JerkProfile_Type1_inf_DSAmax_inf_DSZ1_Z1(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Xf,Vsmp)


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;


  Tjpa = (Amax - A0)/Jmax;
  Tjndc = (Amax - Af)/Jmax;


  x0 =0;
  i = 0;
  do {
    i++;
    Tj=x0;
    Tj_0=Tj;

    cg = (-Xf + V0 * Tjpa + A0 * Tjpa * Tjpa / 0.2e1 + Jmax * pow(Tjpa, 0.3e1) / 0.6e1 +  Vsmp * ( (2 * Vf) -  (2 * Tj * Af) + 0.2e1 * Jmax *  (Tj * Tj) -  (2 * Vsmp) -  (2 * Amax * Tjndc) -  (2 * Amax * Tj) + Jmax *  (Tjndc * Tjndc) + 0.2e1 * Jmax *  Tjndc *  Tj) /  Amax / 0.2e1 + 0.1e1 /  Amax * pow( (2 * Vf) -  (2 * Tj * Af) + 0.2e1 * Jmax *  (Tj * Tj) -  (2 * Vsmp) -  (2 * Amax * Tjndc) -  (2 * Amax * Tj) + Jmax *  (Tjndc * Tjndc) + 0.2e1 * Jmax *  Tjndc *  Tj, 0.2e1) / 0.8e1 + ( Vf -  (Tj * Af) + Jmax *  (Tj * Tj) -  (Amax * Tjndc) -  (Amax * Tj) + Jmax *  (Tjndc * Tjndc) / 0.2e1 + Jmax *  Tjndc *  Tj) *  (Tjndc + Tj) +  (Amax *  pow( (Tjndc + Tj),  2)) / 0.2e1 - Jmax *   pow( (Tjndc + Tj),  3) / 0.6e1 + ( Vf -  (Tj * Af) + Jmax *  (Tj * Tj) -  (Amax * Tjndc) -  (Amax * Tj) + Jmax *  (Tjndc * Tjndc) / 0.2e1 + Jmax *  Tjndc *  Tj +  (Amax * (Tjndc + Tj)) - Jmax *   pow( (Tjndc + Tj),  2) / 0.2e1) *  Tj + ( Amax - Jmax *  (Tjndc + Tj)) *  (Tj * Tj) / 0.2e1 + Jmax *   pow( Tj,  3) / 0.6e1) / (0.2e1 * Jmax *  Tjndc *  Tj -  (2 * Tj * Af) +  (2 * Vf) + 0.2e1 * Jmax *  (Tj * Tj) -  (2 * Amax * Tjndc) -  (2 * Amax * Tj) + Jmax *  (Tjndc * Tjndc) +  (2 * Amax * (Tjndc + Tj)) + 0.1e1 /  Amax * ( Vf -  (Tj * Af) + Jmax *  (Tj * Tj) -  Vsmp -  (Amax * Tjndc) -  (Amax * Tj) + Jmax *  (Tjndc * Tjndc) / 0.2e1 + Jmax *  Tjndc *  Tj) * (- Af + 0.2e1 * Jmax *  Tj -  Amax + Jmax *  Tjndc) + 0.2e1 * ( Amax / 0.2e1 - Jmax *  (Tjndc + Tj) / 0.2e1) *  Tj - Jmax *   pow( (Tjndc + Tj),  2) +  Vsmp * (- Af + 0.2e1 * Jmax *  Tj -  Amax + Jmax *  Tjndc) /  Amax + (- Af + 0.2e1 * Jmax *  Tj -  Amax + Jmax *  Tjndc) *  (Tjndc + Tj) + (- Af + 0.2e1 * Jmax *  Tj + Jmax *  Tjndc - Jmax *  (Tjndc + Tj)) *  Tj);

    x1 = x0 - cg;
    x0=x1;
    Tj=x1;
    if (i > 150) { break;}
  } while (ABS(Tj_0 -Tj) > epsilon);

  Taca =  ((2 * Vf - 2 * Tj * Af + 2 * Jmax * Tj * Tj - 2 * Vsmp - 2 * Amax * Tjndc - 2 * Amax * Tj + Jmax * Tjndc * Tjndc + 2 * Jmax * Tjndc * Tj) / Amax) / 0.2e1;

  Tjna = Tjndc + Tj;
  Tvc = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = Tj;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_Z2(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time, double* dc)
{
  /* Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 2 */
  double Jmax, Amax, Vmax;
  double V5, A5, V6, A6, X6;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Tjn, Tjn1, Co0, Co1, Co2, Co3, Co4;
  int i=0;
  double Tjpa_0;
  double A0, V0, X0, Xf, Af, Vf;
  double x1, x0, t3, t6, t10, t19, Div;
  double epsilon=0.0000000001;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (Xf == *dc) {
    Tjpa = 0;
    Taca = 0;
    Tvc = 0;
    Tjn = (A0 + Amax) / Jmax;
    Tacb = -( PartVel->Vfmp -  PartVel->Vsmm) / Amax;
    Tjpb = (Af + Amax) / Jmax;
    if (Tjn >= Amax/Jmax) {
      Tjnb = Amax/Jmax;
      Tjna = Tjn-Tjnb;
    }
    else {
      Tjna=0;
      Tjnb=Tjn;
    }
  }
  else {
    A5 = -Amax;
    Tjn1 = -(-Amax - A0)/ Jmax;
    Tjpb = (Af + Amax) / Jmax;
    A6 = Af -Jmax*Tjpb;
    V6 = Vf - (Af -Jmax*Tjpb)*Tjpb - (0.5)*Jmax*Tjpb*Tjpb;
    X6 = Xf -  V6*Tjpb - (0.5)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;

    /* Resolution of quartic equation: E = a0 +a1*Tjpa +a2*Tjpa^2+a3*Tjpa^3+a4*Tjpa^4  */
    Co0 = X6-(V0-0.5/Jmax*(Amax*Amax-A0*A0))*(-V6+V0-0.5/Jmax*(Amax*Amax-A0*A0))/Amax+0.5/Amax*(-V6+V0-0.5/Jmax*(Amax*Amax-A0*A0))*(-V6+V0-0.5/Jmax*(Amax*Amax-A0*A0))-(V0-0.5/Jmax*(Amax*Amax-A0*A0)-(-Amax+Jmax*Tjn1)*Tjn1+0.5*Jmax*Tjn1*Tjn1)*Tjn1-(-0.5*Amax+0.5*Jmax*Tjn1)*Tjn1*Tjn1+1.0/6.0*Jmax*Tjn1*Tjn1*Tjn1;
    Co1 = -Amax*Tjn1+0.5/Jmax*Amax*Amax-2*V0-1.0/Amax*A0*A0*A0/Jmax-2.0/Amax*V0*A0+Amax*A0/Jmax-2*A0*Tjn1-0.5/Jmax*A0*A0+0.5*Jmax*Tjn1*Tjn1;
    Co2 = -2.5*A0-2.5/Amax*A0*A0-1.0/Amax*V0*Jmax-0.5*Jmax*Tjn1;
    Co3 = -2.0/Amax*A0*Jmax-Jmax;
    Co4 = -0.5/Amax*Jmax*Jmax;
    /* Resolution par la Methode de Newton */
    /*    E =( Co0 + Co1*Tjpa + Co2*Tjpa*Tjpa + Co3*Tjpa*Tjpa*Tjpa + Co4*Tjpa*Tjpa*Tjpa*Tjpa);
          E' = (Co1 + 2*Co2*Tjpa + 3*Co3*Tjpa*Tjpa + 4*Co4*Tjpa*Tjpa*Tjpa); */

    x0 =2*Amax/Jmax;
    i = 0;
    do {
      i++;
      Tjpa=x0;
      Tjpa_0=Tjpa;
      t3 = Tjpa * Tjpa;
      t6 = t3*Tjpa;
      t10 = 1 / (Co1 + 2 * Co2 * Tjpa + 3 * Co3 * t3 + 4 * Co4 * t6);
      t19 = t3 * t3;
      Div = t10 * Co0 + t10 * Co1 * Tjpa + t10 * Co2 * t3 + t10 * Co3 * t6 + t10 * Co4 * t19;
      x1 = x0 -Div/6;
      if (i<200) {
	if ((ABS(x1) >2*Amax/Jmax)|| x1<0) {
	  x1 = 0;
	}
      }
      else {
	if (i==200) {
	  x1 = Amax/Jmax ;
	}
      }
      if (i>300) { break;}
      //  printf("Tjpa %f\n",Tjpa);
      x0=x1;
      Tjpa=x1;

    } while (ABS(Tjpa_0 -Tjpa) > epsilon);
    //printf("Convergence de Newton en %d iterations\n",i);

    V5 =   V0 + A0*Tjpa + (0.5)*Jmax*Tjpa*Tjpa+ (A0 +Jmax*Tjpa)*Tjpa -(0.5)*Jmax*Tjpa*Tjpa - (1/(2*Jmax))*(Amax*Amax-A0*A0);
    Tacb = (V5 -V6) / Amax;
    Tjn = Tjn1 + Tjpa;
    if (Tjn >= Amax/Jmax) {
      Tjnb = Amax/Jmax;
      Tjna = Tjn-Tjnb;
    }
    else {
      Tjna=0;
      Tjnb=Tjn;
    }
    Tvc = 0;
    Taca= 0;
  }

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_2_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double* DSAmaxT12_2)
{
  // [DSAmaxT12_2]=sm_Calcul_Of_DSAmax2_2_Type1(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf);
  double Jmax, Amax, Vmax;
  double Tjna, Tjpa,Tacb, Tjpb, Tjn;
  double X1=0, A1, V1, A5, V5, X5;
  double A0, V0, Af, Vf;


  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Init Times */
  Tjpa = 0;
  Tjn = 0;
  Tacb = 0;
  Tjpb = 0;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;

  A5 = -Amax;
  V5 = Vf + (1/(2*Jmax))*(Amax*Amax-Af*Af);
  A1 = sqrt( Jmax*(V5-V0) + (0.5)*(A5*A5 + A0*A0));
  V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);

  Tjpa = (A1 - A0)/Jmax;
  X1 = V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;

  Tjna = (A1 - A5)/Jmax;

  X5 = X1 + V1*Tjna + (0.5)*A1*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna ;
  Tjpb = (Af +Amax)/Jmax;
  *DSAmaxT12_2 = X5 + V5*Tjpb + (0.5)*A5*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb ;

  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_22_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double *DSAmaxT12_22)
{
  /* function [DSAmaxT12_22]=sm_Calcul_Of_DSAmax2_22_Type1(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Vs0m,Vfmp)
     Fonction permettant de calculer le profil du jerk pour un chemin type 1
     avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 2 */
  double Jmax, Amax, Vmax;
  double V5, A5;
  double Tjpa, Tjna, Tjpb;
  double A1, V1, X1, X5;
  double A0, V0, X0, Xf, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;


  A5 = -Amax;
  V5 = Vf + (1/(2*Jmax))*(Amax*Amax-Af*Af);
  A1 = -sqrt( Jmax*(V5-V0) + (0.5)*(A5*A5 + A0*A0));
  V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);

  Tjpa = (A1 - A0)/Jmax;
  X1 = V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;

  Tjna = (A1 - A5)/Jmax;

  X5 = X1 + V1*Tjna + (0.5)*A1*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna ;
  Tjpb = (Af +Amax)/Jmax;
  *DSAmaxT12_22 = X5 + V5*Tjpb + (0.5)*A5*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb ;

  /*  A1 = 0; */
  /*   V1 = PartVel->Vs0m; */
  /*   Tjpa = (0-A0)/Jmax; */
  /*   X1 = V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa; */

  /*   V5 = PartVel->Vs0m - (1/(2*Jmax))*(Amax*Amax); */
  /*   A5 = -Amax; */
  /*   Tjna = -(-Amax - 0)/Jmax; */
  /*   X5 = X1 + V1*Tjna + (0.5)*A1*Tjna*Tjna - (1.0/6.0)*Jmax*Tjna*Tjna*Tjna; */

  /*   A6 = -Amax; */
  /*   V6 = PartVel->Vfmp; */

  /*   Tacb =- (V6 - V5)/Amax; */
  /*   X6 = X5 + V5*Tacb - (0.5)*Amax*Tacb*Tacb; */

  /*   Tjpb = (Af +Amax)/Jmax; */
  /*   *DSAmaxT12_22 = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb; */

  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_inf_DSAmaxT12_22_Z2(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  /* [Tjpa,Taca,Tjna,Tvc,Tjnb,Tacb,Tjpb]=sm_JerkProfile_Type1_inf_DSAmax_inf_DSAmaxT12_22_Z2(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Xf,dc,Vfmp,Vsmm);*/

  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Tjndc;
  int i=0;
  double Tj_0, Tj;
  double A0, V0, X0, Xf, Af, Vf;
  double x1, x0,  cg;
  double epsilon=0.00000001;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  Tjpb = (Af + Amax)/Jmax;
  Tjndc = (A0 + Amax)/Jmax;

  //E = -Xf +  V0*Tj + (1/2)*A0*Tj^2 + (1/6)*Jmax*Tj^3 + (V0 + A0*Tj + (1/2)*Jmax*Tj^2)*(Tj+Tjndc) + (1/2)*(A0 + Jmax*Tj)*(Tj+Tjndc)^2 - (1/6)*Jmax*(Tj+Tjndc)^3 + (V0 + A0*Tj + (1/2)*Jmax*Tj^2 + (A0 + Jmax*Tj)*(Tj+Tjndc) - (1/2)*Jmax*(Tj+Tjndc)^2)*(1/2*(-2*Vf*Jmax+2*Jmax*Tjpb*Af-Jmax^2*Tjpb^2-Amax^2+A0^2+4*A0*Jmax*Tj+2*Jmax^2*Tj^2+2*V0*Jmax)/Jmax/Amax) - (1/2)*Amax*(1/2*(-2*Vf*Jmax+2*Jmax*Tjpb*Af-Jmax^2*Tjpb^2-Amax^2+A0^2+4*A0*Jmax*Tj+2*Jmax^2*Tj^2+2*V0*Jmax)/Jmax/Amax)^2 +  (V0 + A0*Tj + (1/2)*Jmax*Tj^2 + (A0 + Jmax*Tj)*(Tj+Tjndc) - (1/2)*Jmax*(Tj+Tjndc)^2 - Amax*(1/2*(-2*Vf*Jmax+2*Jmax*Tjpb*Af-Jmax^2*Tjpb^2-Amax^2+A0^2+4*A0*Jmax*Tj+2*Jmax^2*Tj^2+2*V0*Jmax)/Jmax/Amax))*Tjpb - (1/2)*Amax*Tjpb^2 + (1/6)*Jmax*Tjpb^3;

  //Ep = 2*V0+2*A0*Tj+Jmax*Tj^2+(A0+Jmax*Tj)*(Tj+Tjndc)+2*(1/2*A0+1/2*Jmax*Tj)*(Tj+Tjndc)+(2*A0+2*Jmax*Tj)*(-Vf*Jmax+Jmax*Tjpb*Af-1/2*Jmax^2*Tjpb^2-1/2*Amax^2+1/2*A0^2+2*A0*Jmax*Tj+Jmax^2*Tj^2+V0*Jmax)/Jmax/Amax+(V0+A0*Tj+1/2*Jmax*Tj^2+(A0+Jmax*Tj)*(Tj+Tjndc)-1/2*Jmax*(Tj+Tjndc)^2)*(2*A0*Jmax+2*Jmax^2*Tj)/Jmax/Amax-1/Amax*(-Vf*Jmax+Jmax*Tjpb*Af-1/2*Jmax^2*Tjpb^2-1/2*Amax^2+1/2*A0^2+2*A0*Jmax*Tj+Jmax^2*Tj^2+V0*Jmax)/Jmax^2*(2*A0*Jmax+2*Jmax^2*Tj)+(2*A0+2*Jmax*Tj-(2*A0*Jmax+2*Jmax^2*Tj)/Jmax)*Tjpb;

  x0 =0;
  i = 0;
  do {
    i++;
    Tj=x0;
    Tj_0=Tj;

    cg = (-Xf + V0 * Tj + A0 * Tj * Tj / 0.2e1 + Jmax * pow(Tj, 0.3e1) / 0.6e1 + (V0 + A0 * Tj + Jmax * Tj * Tj / 0.2e1) * (Tj + Tjndc) + (A0 + Jmax * Tj) * pow(Tj + Tjndc, 0.2e1) / 0.2e1 - Jmax * pow(Tj + Tjndc, 0.3e1) / 0.6e1 + (V0 + A0 * Tj + Jmax * Tj * Tj / 0.2e1 + (A0 + Jmax * Tj) * (Tj + Tjndc) - Jmax * pow(Tj + Tjndc, 0.2e1) / 0.2e1) * (-0.2e1 * Vf * Jmax + 0.2e1 * Jmax * Tjpb * Af - Jmax * Jmax * Tjpb * Tjpb - Amax * Amax + A0 * A0 + 0.4e1 * A0 * Jmax * Tj + 0.2e1 * Jmax * Jmax * Tj * Tj + 0.2e1 * V0 * Jmax) / Jmax / Amax / 0.2e1 - 0.1e1 / Amax * pow(-0.2e1 * Vf * Jmax + 0.2e1 * Jmax * Tjpb * Af - Jmax * Jmax * Tjpb * Tjpb - Amax * Amax + A0 * A0 + 0.4e1 * A0 * Jmax * Tj + 0.2e1 * Jmax * Jmax * Tj * Tj + 0.2e1 * V0 * Jmax, 0.2e1) * pow(Jmax, -0.2e1) / 0.8e1 + (V0 + A0 * Tj + Jmax * Tj * Tj / 0.2e1 + (A0 + Jmax * Tj) * (Tj + Tjndc) - Jmax * pow(Tj + Tjndc, 0.2e1) / 0.2e1 - (-0.2e1 * Vf * Jmax + 0.2e1 * Jmax * Tjpb * Af - Jmax * Jmax * Tjpb * Tjpb - Amax * Amax + A0 * A0 + 0.4e1 * A0 * Jmax * Tj + 0.2e1 * Jmax * Jmax * Tj * Tj + 0.2e1 * V0 * Jmax) / Jmax / 0.2e1) * Tjpb - Amax * Tjpb * Tjpb / 0.2e1 + Jmax * pow(Tjpb, 0.3e1) / 0.6e1) / (0.2e1 * V0 + 0.2e1 * A0 * Tj + Jmax * Tj * Tj + (A0 + Jmax * Tj) * (Tj + Tjndc) + 0.2e1 * (A0 / 0.2e1 + Jmax * Tj / 0.2e1) * (Tj + Tjndc) + (0.2e1 * A0 + 0.2e1 * Jmax * Tj) * (-Vf * Jmax + Jmax * Tjpb * Af - Jmax * Jmax * Tjpb * Tjpb / 0.2e1 - Amax * Amax / 0.2e1 + A0 * A0 / 0.2e1 + 0.2e1 * A0 * Jmax * Tj + Jmax * Jmax * Tj * Tj + V0 * Jmax) / Jmax / Amax + (V0 + A0 * Tj + Jmax * Tj * Tj / 0.2e1 + (A0 + Jmax * Tj) * (Tj + Tjndc) - Jmax * pow(Tj + Tjndc, 0.2e1) / 0.2e1) * (0.2e1 * A0 * Jmax + 0.2e1 * Jmax * Jmax * Tj) / Jmax / Amax - 0.1e1 / Amax * (-Vf * Jmax + Jmax * Tjpb * Af - Jmax * Jmax * Tjpb * Tjpb / 0.2e1 - Amax * Amax / 0.2e1 + A0 * A0 / 0.2e1 + 0.2e1 * A0 * Jmax * Tj + Jmax * Jmax * Tj * Tj + V0 * Jmax) * pow(Jmax, -0.2e1) * (0.2e1 * A0 * Jmax + 0.2e1 * Jmax * Jmax * Tj) + (0.2e1 * A0 + 0.2e1 * Jmax * Tj - (0.2e1 * A0 * Jmax + 0.2e1 * Jmax * Jmax * Tj) / Jmax) * Tjpb);

    x1 = x0 - cg/10;
    x0=x1;
    // if ( isnan(x1))  {
    if (ABS(x0) > 2*Amax/Jmax) {
      x0 =2*Amax/Jmax;
    }
    Tj=x1;
    //  printf("Tj %f\n",Tj);
    if (i > 300) { break;}
    // //printf("x1 %f\n",x1);
  } while (ABS(Tj_0 -Tj) > epsilon);
  //printf("Convergence de Newton en %d iterations\n",i);

  Tjpa = Tj;
  Taca = 0;
  Tjna = Tjndc + Tj;
  Tvc = 0;
  Tjnb = 0;
  //Tacb = (1/2*(-2*Vf*Jmax+2*Jmax*Tjpb*Af-Jmax^2*Tjpb^2-Amax^2+A0^2+4*A0*Jmax*Tj+2*Jmax^2*Tj^2+2*V0*Jmax)/Jmax/Amax);

  Tacb=((-2*Vf*Jmax+2*Jmax*Tjpb*Af-Jmax*Jmax*Tjpb*Tjpb-Amax*Amax+A0*A0+4*A0*Jmax*Tj+2*Jmax*Jmax*Tj*Tj+2*V0*Jmax)/Jmax/Amax)/0.2e1;


  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}


SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_Z3(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_TIMES* Time)
{
  /*Fonction permettant de calculer le profil du jerk pour un chemin type 1
    avec Xf < DSVmax , Xf < DSAmax et on est dans le cas zone 3
    function [Tjpa,Taca,Tjna,Tvc,Tjnb,Tacb,Tjpb]=sm_JerkProfile_Type1_inf_DSAmax_Z3(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Xf,dc,Vfmm,Vsmp)*/
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double A1dc, V1dc, Tjndc, Tjpdc,Tj1, Tj2;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf;
  double x0, x1, cg;
  double Tj1_0;
  double epsilon = 0.00000000001;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  // printf(" sm_JerkProfile_Type1_inf_DSAmax_Z3\n");

  //  printf("total %f\n", Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af));
  A1dc = sqrt(ABS( Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af)));
  V1dc = V0 + (1.0/(2*Jmax))*(A1dc*A1dc - A0*A0);
  Tjpdc = (A1dc - A0) / Jmax ;
  Tjndc= (A1dc - Af) / Jmax;
  // printf("Vf %f V0 %f A0 %f Af %f A1dc %f V1dc %f Tjpdc %f Tjndc %f\n",Vf,V0,A0,Af,A1dc,V1dc,Tjpdc,Tjndc);
  x0 =2*Amax/Jmax;
  i = 0;
  do {
    i++;
    Tj1=x0;
    Tj1_0=Tj1;
    cg = (-Xf + X0 + V0 * (Tjpdc + Tj1) + A0 * pow(Tjpdc + Tj1, 0.2e1) / 0.2e1 + Jmax * pow(Tjpdc + Tj1, 0.3e1) / 0.6e1 + (V0 + A0 * (Tjpdc + Tj1) + Jmax * pow(Tjpdc + Tj1, 0.2e1) / 0.2e1) * (Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) + (A0 / 0.2e1 + Jmax * (Tjpdc + Tj1) / 0.2e1) * pow(Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) - Jmax * pow(Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.3e1) / 0.6e1 + (Vf + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) * ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax / 0.2e1 - 0.1e1 / Jmax * pow( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) / 0.2e1) * ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) * pow( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) * pow(Jmax, -0.2e1) / 0.4e1 + pow(Jmax, -0.2e1) * pow( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.3e1) / 0.6e1) / (V0 + A0 * (Tjpdc + Tj1) + Jmax * pow(Tjpdc + Tj1, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpdc + Tj1)) * (Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) + (V0 + A0 * (Tjpdc + Tj1) + Jmax * pow(Tjpdc + Tj1, 0.2e1) / 0.2e1) * (0.1e1 + pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) + Jmax * pow(Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpdc + Tj1)) * (Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) * (0.1e1 + pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) - Jmax * pow(Tjndc + Tj1 + ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) * (0.1e1 + pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) / 0.2e1 + (Vf / 0.4e1 + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) * ( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax / 0.8e1 - 0.1e1 / Jmax * pow( Af + sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) / 0.8e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax);

    x1 = x0 - cg/2;

    if ( isnan(x1))  {
      x0 = 0;
      x1 = 0;
      Tj1=0;
    }
    else {
      x0=x1;
      Tj1=x1;
    }
    // printf("x1 %f\n",x1);
    if (i > 300) { break;}
    // //printf("x1 %f\n",x1);
  } while (ABS(Tj1_0 -Tj1) > epsilon);
  //printf("Convergence de Newton en %d iterations\n",i);

  Tj2=(Af+0.5*sqrt(2*Af*Af-4*Vf*Jmax+4*Jmax*Jmax*Tj1*Tj1+4*V1dc*Jmax+8*A1dc*Jmax*Tj1+2*A1dc*A1dc))/Jmax;
  Tjpa = Tjpdc + Tj1;
  Taca = 0;
  Tjna = Tjndc + Tj1 + Tj2;
  Tvc  = 0;
  Tacb = 0;
  Tjpb = Tj2;
  Tjna = Tjndc + Tj1 + Tj2;
  Tjnb = 0;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_31_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double* DSAmaxT12_31)
{
  double Jmax, Amax, Vmax;
  double Tjpa,Tacb, Tjpb, Tjn;
  double X1=0, A1, V1, A2, V2, X2, A5, V5, X5, A6, V6, X6;
  double A0, V0, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Init Times */
  Tjpa = 0;
  Tjn = 0;
  Tacb = 0;
  Tjpb = 0;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;

  if (PartVel->Vs0m <= PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (1.0/2.0)*(A0*A0));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }
  A2 = A1;
  V2 = V1;
  X2 = X1;
  A5 = - sqrt( Jmax*(V2 - Vf) + (1.0/2.0)*(A2*A2 + Af*Af));
  V5 = Vf + (1.0/(2*Jmax))*(A5*A5 - Af*Af);
  Tjn = - (A5 - A2) / Jmax;
  X5 = X2 + V2*Tjn + (1.0/2.0)*A2*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
  A6 = A5;
  V6 = V5;
  X6 = X5;
  Tacb = 0;
  Tjpb = (Af - A6) / Jmax;
  *DSAmaxT12_31 = X6 + V6*Tjpb + (1.0/2.0)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_311_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double* DSAmaxT12_311)
{
  double Jmax, Amax, Vmax;
  double Tjpa,Tacb, Tjpb, Tjn;
  double X1=0, A1, V1, A2, V2, X2, A5, V5, X5, A6, V6, X6;
  double A0, V0, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Init Times */
  Tjpa = 0;
  Tjn = 0;
  Tacb = 0;
  Tjpb = 0;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;

  if (PartVel->Vs0m <= PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (1.0/2.0)*(A0*A0));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }
  A2 = A1;
  V2 = V1;
  X2 = X1;
  A5 = sqrt( Jmax*(V2 - Vf) + (1.0/2.0)*(A2*A2 + Af*Af));
  V5 = Vf + (1.0/(2*Jmax))*(A5*A5 - Af*Af);
  Tjn = - (A5 - A2) / Jmax;
  X5 = X2 + V2*Tjn + (1.0/2.0)*A2*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
  A6 = A5;
  V6 = V5;
  X6 = X5;
  Tacb = 0;
  Tjpb = (Af - A6) / Jmax;
  *DSAmaxT12_311 = X6 + V6*Tjpb + (1.0/2.0)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_32_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double* DSAmaxT12_32)
{
  double Jmax, Amax, Vmax;
  double Tjpa,Tacb, Tjpb, Tjn;
  double A1, V1, A2, V2, X2, A6, V6, X6;
  double A0, V0, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Init Times */
  Tjpa = 0;
  Tjn = 0;
  Tacb = 0;
  Tjpb = 0;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;

  if (PartVel->Vf0m <= PartVel->Vlim) {
    A6 = -Amax;
    V6 = Vf + (1/(2*Jmax))*(A6*A6 - Af*Af);
  }
  else {
    A6 = -sqrt( Jmax*(Vmax - Vf) + (1.0/2.0)*(Af*Af));
    V6 = Vf + (1.0/(2*Jmax))*(A6*A6 - Af*Af);
  }
  Tjpb = (Af - A6) / Jmax;
  A2 = sqrt( Jmax*(V6 - V0) + (1.0/2.0)*(A6*A6 + A0*A0) );
  V2 = V6 - (1/(2*Jmax))*(A2*A2 - A6*A6);
  A1=A2;
  V1=V2;
  Tjpa = (A1 - A0) / Jmax;
  X2 = V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  Tjn = - (A6 - A2) / Jmax;
  X6 = X2 + V2*Tjn + (1.0/2.0)*A2*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
  *DSAmaxT12_32 = X6 + V6*Tjpb + (1.0/2.0)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  return SM_OK;
}
SM_STATUS sm_Calcul_Of_DSAmax2_33_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, double* DSAmaxT12_33)
{
  double Jmax, Amax, Vmax;
  double T1,T2,T3;
  double X1=0, A1, V1, Aint, Vint, Xint;
  double A0, V0, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;

  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;

  Aint = 0;
  Vint = Vf - (1.0/(2*Jmax))*Af*Af;
  A1 = sqrt(Jmax*(Vint - V0) + (0.5)*A0*A0);
  V1 = V0 + (1.0/(2*Jmax))*(A1*A1 -A0*A0);
  T1 = (A1 -A0)/Jmax ;
  T2 = -(Aint - A1)/Jmax;
  T3 = (Af -Aint)/Jmax;
  X1 = V0*T1 + (0.5)*A0*T1*T1 +(1.0/6.0)*Jmax*T1*T1*T1;
  Xint = X1 + V1*T2 + (0.5)*A1*T2*T2 - (1.0/6.0)*Jmax*T2*T2*T2;
  *DSAmaxT12_33 = Xint + Vint*T3 + (0.5)*Aint*T3*T3 + (1.0/6.0)*Jmax*T3*T3*T3;
  return SM_OK;
}

SM_STATUS sm_Calcul_Of_DSAmax2_4_Type1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, double *dc, double* DSAmaxT12_4)
{
  /*[DSAmaxT12_4]=sm_Calcul_Of_DSAmax2_4_Type1(Amax,Vmax,Jmax,A0,V0,X0,Af,Vf,Xf,Vs0m,Vlim,Vfmp,Vsmm,dc)*/
  double Jmax, Amax, Vmax;
  double Tjpa, Tjn, Tacb, Tjpb;
  double X1, A1, V1, A5, V5, X5, A6, V6, X6;
  double X0, A0, V0, Af, Vf;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;

  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  Vf = FCm->v;
  Af = FCm->a;
  X0=0;

  if (PartVel->Vfmp  > PartVel->Vsmm) {
    A5 = -Amax;
    V5 = PartVel->Vfmp;

    A1 = sqrt( Jmax*(V5-V0) + (0.5)*(A5*A5+A0*A0));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);

    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (0.5)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
    Tjn = - (A5 - A1) / Jmax;
    X5 = X1 + V1*Tjn + (0.5)*A1*Tjn*Tjn - (1.0/6.0)*Jmax*Tjn*Tjn*Tjn;
    A6 = A5;
    V6 = V5;
    X6 = X5;
    Tacb = 0;
    Tjpb = (Af - A6) / Jmax;
    *DSAmaxT12_4 = X6 + V6*Tjpb + (0.5)*A6*Tjpb*Tjpb + (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  }
  else {

    *DSAmaxT12_4 = *dc;

  }

  return SM_OK;
}


SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_31_Z3(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb, Tjn;
  double A5dc, V5dc ;
  double Tjpb1, Tjn1, Tjpb2, cg;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf;
  double  X1, A1, V1, V2 ;
  double x1, x0;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  if (PartVel->Vs0m <= PartVel->Vlim) {
    A1 = Amax;
    V1 = V0 + (1.0/(2*Jmax))*(Amax*Amax - A0*A0);
    Tjpa = (Amax - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }
  else {
    A1 = sqrt( Jmax*(Vmax - V0) + (1.0/2.0)*(A0*A0));
    V1 = V0 + (1.0/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax;
    X1 = X0 + V0*Tjpa + (1.0/2.0)*A0*Tjpa*Tjpa + (1.0/6.0)*Jmax*Tjpa*Tjpa*Tjpa;
  }

  A5dc = - sqrt( Jmax*(V1 - Vf) + (1.0/2.0)*(A1*A1 + Af*Af));
  V5dc = Vf + (1.0/(2*Jmax))*(A5dc*A5dc - Af*Af);
  Tjn1 = (Amax - A5dc) / Jmax;
  Tjpb1 = (Af - A5dc) / Jmax;

  /* Resolution par la Methode de Newton */
  //printf("coucou\n");
  x0 =Amax/Jmax;
  for (i=1;i<25;i++) {
    Tjpb2=x0;
    cg = (-Xf + X1 + V1 * (V5dc - (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - pow(A5dc - Jmax * Tjpb2, 0.2e1)) / 0.2e1 - V1) / Amax + 0.1e1 / Amax * pow(V5dc - (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - pow(A5dc - Jmax * Tjpb2, 0.2e1)) / 0.2e1 - V1, 0.2e1) / 0.2e1 + (V5dc - (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - pow(A5dc - Jmax * Tjpb2, 0.2e1)) / 0.2e1) * (Tjn1 + Tjpb2) + Amax * pow(Tjn1 + Tjpb2, 0.2e1) / 0.2e1 - Jmax * pow(Tjn1 + Tjpb2, 0.3e1) / 0.6e1 + (V5dc - (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 / 0.2e1) * (Tjpb2 + Tjpb1) + (A5dc - Jmax * Tjpb2) * pow(Tjpb2 + Tjpb1, 0.2e1) / 0.2e1 + Jmax * pow(Tjpb2 + Tjpb1, 0.3e1) / 0.6e1) / (V1 * (0.2e1 * Jmax * Tjpb2 - 0.2e1 * A5dc) / Amax + 0.1e1 / Amax * (V5dc - (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - pow(A5dc - Jmax * Tjpb2, 0.2e1)) / 0.2e1 - V1) * (0.2e1 * Jmax * Tjpb2 - 0.2e1 * A5dc) + (0.2e1 * Jmax * Tjpb2 - 0.2e1 * A5dc) * (Tjn1 + Tjpb2) + 0.2e1 * V5dc - 0.2e1 * (A5dc - Jmax * Tjpb2) * Tjpb2 - Jmax * Tjpb2 * Tjpb2 - 0.1e1 / Jmax * (Amax * Amax - pow(A5dc - Jmax * Tjpb2, 0.2e1)) / 0.2e1 + Amax * (Tjn1 + Tjpb2) - Jmax * pow(Tjn1 + Tjpb2, 0.2e1) / 0.2e1 + (Jmax * Tjpb2 - A5dc) * (Tjpb2 + Tjpb1) + 0.2e1 * (A5dc / 0.2e1 - Jmax * Tjpb2 / 0.2e1) * (Tjpb2 + Tjpb1));
    x1 = x0 - cg/1.8;
    x0=x1;
    Tjpb2=x1;
  }

  V2 = V5dc - (A5dc - Jmax*Tjpb2)*Tjpb2 -(1.0/2.0)*Jmax*Tjpb2*Tjpb2- (1.0/(2*Jmax))*(Amax*Amax-(A5dc - Jmax*Tjpb2)*(A5dc - Jmax*Tjpb2));
  Taca = (V2-V1)/Amax;
  Tjn = Tjn1 + Tjpb2;
  if (Tjn >= Amax/Jmax) {
    Tjna = Amax/Jmax;
    Tjnb = Tjn-Tjna;
  }
  else {
    Tjnb=0;
    Tjna=Tjn;
  }
  Tvc = 0;
  Tacb= 0;
  Tjpb = Tjpb1 +Tjpb2;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_32_Z3(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, SM_TIMES* Time)
{
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb, Tjn;
  double A1dc, V1dc ;
  double Tjpa1, Tjn1, Tjpa2, cg;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf;
  double A6, V6, X6, V5 ;
  double x1, x0;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  X0=0;
  if (PartVel->Vf0m <= PartVel->Vlim) {
    A6 = -Amax;
    V6 = Vf + (1.0/(2*Jmax))*(A6*A6 - Af*Af);
  }
  else {
    A6 = -sqrt( Jmax*(Vmax - Vf) + (1.0/2.0)*(Af*Af));
    V6 = Vf + (1.0/(2*Jmax))*(A6*A6 - Af*Af);
  }
  Tjpb = (Af - A6) / Jmax;
  X6 = Xf -  V6*Tjpb - (1.0/2.0)*A6*Tjpb*Tjpb - (1.0/6.0)*Jmax*Tjpb*Tjpb*Tjpb;
  A1dc = sqrt( Jmax*(V6 - V0) + (1.0/2.0)*(A6*A6 + A0*A0) );
  V1dc = V6 - (1.0/(2*Jmax))*(A1dc*A1dc - A6*A6);
  Tjpa1 = (A1dc - A0) / Jmax;
  Tjn1 = - (A6 - A1dc) / Jmax;

  /* Resolution par la Methode de Newton */
  x0 =0;
  for (i=1;i<30;i++) {
    Tjpa2=x0;
    cg = (X6 - (V0 + A0 * (Tjpa1 + Tjpa2) + Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 - Jmax * Tjpa2 * Tjpa2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1) * (V0 + A0 * (Tjpa1 + Tjpa2) + Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 - Jmax * Tjpa2 * Tjpa2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1 - V6) / Amax + 0.1e1 / Amax * pow(V0 + A0 * (Tjpa1 + Tjpa2) + Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 - Jmax * Tjpa2 * Tjpa2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1 - V6, 0.2e1) / 0.2e1 - (V0 + A0 * (Tjpa1 + Tjpa2) + Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 - Jmax * Tjpa2 * Tjpa2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1 - (-Amax + Jmax * (Tjn1 + Tjpa2)) * (Tjn1 + Tjpa2) + Jmax * pow(Tjn1 + Tjpa2, 0.2e1) / 0.2e1) * (Tjn1 + Tjpa2) - (-Amax + Jmax * (Tjn1 + Tjpa2)) * pow(Tjn1 + Tjpa2, 0.2e1) / 0.2e1 + Jmax * pow(Tjn1 + Tjpa2, 0.3e1) / 0.6e1 - V0 * (Tjpa1 + Tjpa2) - A0 * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 - Jmax * pow(Tjpa1 + Tjpa2, 0.3e1) / 0.6e1) / (-(V0 + A0 * (Tjpa1 + Tjpa2) + Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) / 0.2e1 + (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 - Jmax * Tjpa2 * Tjpa2 / 0.2e1 - 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1) * (0.2e1 * A0 + 0.2e1 * Jmax * (Tjpa1 + Tjpa2)) / Amax - (0.2e1 * A0 + 0.2e1 * Jmax * (Tjpa1 + Tjpa2) - Jmax * (Tjn1 + Tjpa2) + Amax) * (Tjn1 + Tjpa2) - 0.2e1 * V0 - 0.2e1 * A0 * (Tjpa1 + Tjpa2) - Jmax * pow(Tjpa1 + Tjpa2, 0.2e1) - (A0 + Jmax * (Tjpa1 + Tjpa2)) * Tjpa2 + Jmax * Tjpa2 * Tjpa2 / 0.2e1 + 0.1e1 / Jmax * (Amax * Amax - A1dc * A1dc) / 0.2e1 + (-Amax + Jmax * (Tjn1 + Tjpa2)) * (Tjn1 + Tjpa2) - Jmax * pow(Tjn1 + Tjpa2, 0.2e1) / 0.2e1 - 0.2e1 * (-Amax / 0.2e1 + Jmax * (Tjn1 + Tjpa2) / 0.2e1) * (Tjn1 + Tjpa2));
    x1 = x0 - cg/1.8;
    x0=x1;
    Tjpa2=x1;
  }

  V5 = ((V0 + A0*(Tjpa1+Tjpa2) + (1.0/2.0)*Jmax*(Tjpa1+Tjpa2)*(Tjpa1+Tjpa2)) + (A0 +Jmax*(Tjpa1+Tjpa2))*Tjpa2 -(1.0/2.0)*Jmax*Tjpa2*Tjpa2) - (1.0/(2*Jmax))*(Amax*Amax-A1dc*A1dc);
  Tacb = (V5-V6) / Amax;
  Tjn = Tjn1 + Tjpa2;
  if (Tjn >= Amax/Jmax) {
    Tjna = Amax/Jmax;
    Tjnb = Tjn-Tjna;
  }
  else {
    Tjnb=0;
    Tjna=Tjn;
  }
  Tvc = 0;
  Taca= 0;
  Tjpa = Tjpa1+Tjpa2;

  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;
  return SM_OK;
}

SM_STATUS sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_33_Z3(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm,  SM_TIMES* Time)
{
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  double Tjp1, Tjdc,T1, T2, T3,Tj1, Tj2, X1dc, A1dc,V1dc, Aint, Vint, Xint;
  double A1, V1, X1 ;
  double cg, Tj1_0;
  int i=0;
  double A0, V0, X0, Xf, Af, Vf;
  double x1, x0;
  double Vint0;
  double DSZ32;
  double Amaxdc;
  double epsilon = 0.0000000000000000001;

  double coef =12;
  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = 0.0;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa = 0;
  Taca = 0;
  Tjna = 0;
  Tvc  = 0;
  Tjnb = 0;
  Tacb = 0;
  Tjpb = 0;

  X0=0;

  Vint0 = Vf - (1.0/(2*Jmax))*Af*Af;
  A1dc = sqrt(Jmax*(Vint0 - V0) + (0.5)*A0*A0);
  V1dc = V0 + (1.0/(2*Jmax))*(A1dc*A1dc -A0*A0);
  Tjp1 = (A1dc - A0)/Jmax;
  X1dc = V0*Tjp1 + (0.5)*A0*Tjp1*Tjp1 +(1.0/6.0)*Jmax*Tjp1*Tjp1*Tjp1;

  if ( Af >= A1dc) {

    /*Calcul du nouveau seuil */
    Aint = A1dc;
    Vint = Vf + (1/(2*Jmax))*(Aint*Aint - Af*Af);
    A1 = sqrt(Jmax*(Vint - V0) + (0.5)*(A0*A0+Aint*Aint));
    V1 = V0 + (1.0/(2*Jmax))*(A1*A1 -A0*A0);
    T1 = (A1 -A0)/Jmax;
    T2 = -(Aint - A1)/Jmax;
    T3 = (Af -Aint)/Jmax;
    X1 = V0*T1 + 0.5*A0*T1*T1 +(1.0/6.0)*Jmax*T1*T1*T1;
    Xint = X1 + V1*T2 + (0.5)*A1*T2*T2 - (1.0/6.0)*Jmax*T2*T2*T2;
    DSZ32 = Xint + Vint*T3 + (0.5)*Aint*T3*T3 + (1.0/6.0)*Jmax*T3*T3*T3;

    if (Xf > DSZ32) {
      //   printf("Cas Zarbi 1\n");
      //fprintf(stderr, "A0 = %g\n", ICm->a);
      //fprintf(stderr, "V0 = %g\n", ICm->v);
      //fprintf(stderr, "X0 = %g\n", ICm->x);
      //fprintf(stderr, "Af = %g\n", FCm->a);
      //fprintf(stderr, "Vf = %g\n", FCm->v);
      //fprintf(stderr, "Xf = %g\n", FCm->x);
      //fprintf("DSZ32 %f\n",DSZ32);
      Tjdc = (Af - A1dc)/Jmax;

      Amaxdc = sqrt(Jmax*(Vf - V0) + (0.5)*(A0*A0+Af*Af));
      x0 =(Amaxdc-A1dc);

      i = 0;
      do {
	i++;
	Tj1=x0;
	Tj1_0=Tj1;
	cg = (-Xf + X1dc + V1dc * Tj1 + A1dc * Tj1 * Tj1 / 0.2e1 + Jmax * pow(Tj1, 0.3e1) / 0.6e1 + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1) * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) + (A1dc + Jmax * Tj1) * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) / 0.2e1 - Jmax * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.3e1) / 0.6e1 + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) - Jmax * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) / 0.2e1) * ((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) + (A1dc + Jmax * Tj1 - Jmax * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax)) * pow((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) / 0.2e1 + Jmax * pow((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.3e1) / 0.6e1) / (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) + Jmax * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) / 0.2e1 + 0.2e1 * (A1dc / 0.2e1 + Jmax * Tj1 / 0.2e1) * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) - Jmax * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) / 0.2e1 + (A1dc + Jmax * Tj1 + Jmax * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) + (A1dc + Jmax * Tj1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) - Jmax * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1)) * ((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) - (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) - Jmax * pow(Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax, 0.2e1) / 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1 + (Jmax / 0.2e1 - Jmax * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) / 0.2e1) * pow((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) - (A1dc / 0.2e1 + Jmax * Tj1 / 0.2e1 - Jmax * (Tj1 + (-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax) / 0.2e1) * ((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.2e1 - pow((-Jmax * Tjdc +  Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tj1) / 0.8e1);

	x1 = x0 - cg/coef;
	//	printf("Tj1 %f\n",Tj1);
	if (isnan(x1)) {
	  x1 = Amax/Jmax;
	  x0 = Amax/Jmax;
	  Tj1 = Amax/Jmax;
	  coef = 20;
	}
	x0=x1;
	/* 	if (x0 >(Amaxdc-A1dc) ) { */
	/* 	  x0 = 0; */
	/* 	} */
	Tj1=x1;

	if (i==100) { coef =20; x0 =(Amaxdc-A1dc); }
	if (i>300) { break;}
      } while (ABS(Tj1_0 -Tj1) > epsilon);

      //printf("Convergence de Newton en %d iterations\n",i);
      //printf("Tj1 %f  Tj2 %f\n",Tj1,Tj2);
      Tj2 = (- (Jmax * Tjdc)+Af - sqrt( (2 * Af*Af - 4*Vf*Jmax + 8*A1dc*Jmax*Tj1 + 4 * Jmax * Jmax * Tj1 * Tj1 + 4 * V1dc * Jmax + 2 * A1dc * A1dc)) / 0.2e1) /  Jmax;
      //  	printf("Tj2 %f\n",Tj2);
      Tjpa = Tjp1 + Tj1;
      Taca = 0;
      Tjna = Tj1+Tj2;
      Tjnb = 0;
      Tacb = 0;
      Tjpb = Tj2+Tjdc;
      Tvc = 0;
    }
    else {
      //  printf("Cas Zarbi 2\n");
      Tjdc = (Af - A1dc)/Jmax;
      Amaxdc = sqrt(Jmax*(Vf - V0) + (0.5)*(A0*A0+Af*Af));
      x0 =0;

      i = 0;
      do {
	i++;
	Tj1=x0;
	Tj1_0=Tj1;
	cg = (-Xf + X1dc + V1dc * (Tjdc + Tj1) + A1dc * pow(Tjdc + Tj1, 0.2e1) / 0.2e1 + Jmax * pow(Tjdc + Tj1, 0.3e1) / 0.6e1 + (V1dc + A1dc * (Tjdc + Tj1) + Jmax * pow(Tjdc + Tj1, 0.2e1) / 0.2e1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) + (A1dc + Jmax * (Tjdc + Tj1)) * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.2e1) / 0.2e1 - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.3e1) / 0.6e1 + (V1dc + A1dc * (Tjdc + Tj1) + Jmax * pow(Tjdc + Tj1, 0.2e1) / 0.2e1 + (A1dc + Jmax * (Tjdc + Tj1)) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.2e1) / 0.2e1) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax + (A1dc + Jmax * (Tjdc + Tj1) - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax)) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1, 0.2e1) * pow(Jmax, -0.2e1) / 0.2e1 + pow(Jmax, -0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1, 0.3e1) / 0.6e1) / (V1dc + A1dc * (Tjdc + Tj1) + Jmax * pow(Tjdc + Tj1, 0.2e1) / 0.2e1 + (A1dc + Jmax * (Tjdc + Tj1)) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) + (V1dc + A1dc * (Tjdc + Tj1) + Jmax * pow(Tjdc + Tj1, 0.2e1) / 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) + Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.2e1) / 0.2e1 + 0.2e1 * (A1dc / 0.2e1 + Jmax * (Tjdc + Tj1) / 0.2e1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) / 0.2e1 + (A1dc + Jmax * (Tjdc + Tj1) + Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) + (A1dc + Jmax * (Tjdc + Tj1)) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1)) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax - (V1dc + A1dc * (Tjdc + Tj1) + Jmax * pow(Tjdc + Tj1, 0.2e1) / 0.2e1 + (A1dc + Jmax * (Tjdc + Tj1)) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax, 0.2e1) / 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1 + (Jmax / 0.2e1 - Jmax * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / Jmax / 0.4e1) / 0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1, 0.2e1) * pow(Jmax, -0.2e1) - (A1dc / 0.2e1 + Jmax * (Tjdc + Tj1) / 0.2e1 - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) / Jmax) / 0.2e1) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1) * pow(Jmax, -0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / 0.2e1 - pow(Jmax, -0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1) / 0.2e1, 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.4e1 * Jmax * Jmax * Tjdc * Tjdc + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tjdc + 0.2e1 * A1dc * A1dc + 0.8e1 * Jmax * Jmax * Tjdc * Tj1 + 0.4e1 * Jmax * Jmax * Tj1 * Tj1, -0.1e1 / 0.2e1) * (0.8e1 * A1dc * Jmax + 0.8e1 * Jmax * Jmax * Tjdc + 0.8e1 * Jmax * Jmax * Tj1) / 0.8e1);

	x1 = x0 - cg/7;
	if (isnan(x1)) {
	  x1 = (Amaxdc-A1dc);
	}
	x0=x1;
	if (ABS(x0) > 2*Amax/Jmax ) {
	  x0 = (Amaxdc-A1dc);
	}
	Tj1=x1;
	if (i>100) { break;}
      } while (ABS(Tj1_0 -Tj1) > epsilon);
      //printf("Convergence de Newton en %d iterations\n",i);

      Tj2 = ( Af - sqrt( (2 * Af * Af - 4 * Vf * Jmax + 8 * A1dc * Jmax * Tj1 + 4 * Jmax * Jmax * Tjdc * Tjdc + 4 * V1dc * Jmax + 8 * A1dc * Jmax * Tjdc + 2 * A1dc * A1dc + 8 * Jmax * Jmax * Tjdc * Tj1 + 4 * Jmax * Jmax * Tj1 * Tj1)) / 0.2e1) /  Jmax;
      Tjpa = Tjp1 + +Tjdc + Tj1;
      Taca = 0;
      Tjna = Tj1+Tj2;
      Tjnb = 0;
      Tacb = 0;
      Tjpb = Tj2;
      Tvc = 0;
    }
  }

  else {
    // printf("Cas Zarbi 3\n");
    //fprintf(stderr, "ICm->a = %3.15g;\n", ICm->a);
    //fprintf(stderr, "ICm->v = %3.15g;\n", ICm->v);
    //fprintf(stderr, "ICm->x = %3.15g;\n", ICm->x);
    //fprintf(stderr, "FCm->a = %3.15g;\n", FCm->a);
    //fprintf(stderr, "FCm->v = %3.15g;\n", FCm->v);
    //fprintf(stderr, "FCm->x = %3.15g;\n", FCm->x);

    Amaxdc = sqrt(Jmax*(Vf - V0) + (0.5)*(A0*A0+Af*Af));

    Tjdc = (A1dc-Af)/Jmax;
    x0 =0;
    i = 0;
    do {
      i++;
      Tj1=x0;
      Tj1_0=Tj1;
      cg = (-Xf + X1dc + V1dc * Tj1 + A1dc * Tj1 * Tj1 / 0.2e1 + Jmax * pow(Tj1, 0.3e1) / 0.6e1 + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) + (A1dc + Jmax * Tj1) * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) / 0.2e1 - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.3e1) / 0.6e1 + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) / 0.2e1) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + (A1dc + Jmax * Tj1 - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc)) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) * pow(Jmax, -0.2e1) / 0.2e1 + pow(Jmax, -0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.3e1) / 0.6e1) / (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) + (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) + Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) / 0.2e1 + 0.2e1 * (A1dc / 0.2e1 + Jmax * Tj1 / 0.2e1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) / 0.2e1 + (A1dc + Jmax * Tj1 + Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) + (A1dc + Jmax * Tj1) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1)) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax - (V1dc + A1dc * Tj1 + Jmax * Tj1 * Tj1 / 0.2e1 + (A1dc + Jmax * Tj1) * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) - Jmax * pow(Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc, 0.2e1) / 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1 + (Jmax / 0.2e1 - Jmax * (0.1e1 - pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / Jmax / 0.4e1) / 0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) * pow(Jmax, -0.2e1) - (A1dc / 0.2e1 + Jmax * Tj1 / 0.2e1 - Jmax * (Tj1 + ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) / Jmax + Tjdc) / 0.2e1) * ( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1) * pow(Jmax, -0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / 0.2e1 - pow(Jmax, -0.2e1) * pow( Af - sqrt( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc) / 0.2e1, 0.2e1) * pow( (2 * Af * Af) - 0.4e1 * Vf * Jmax + 0.4e1 * Jmax * Jmax * Tj1 * Tj1 + 0.4e1 * V1dc * Jmax + 0.8e1 * A1dc * Jmax * Tj1 + 0.2e1 * A1dc * A1dc, -0.1e1 / 0.2e1) * (0.8e1 * Jmax * Jmax * Tj1 + 0.8e1 * A1dc * Jmax) / 0.8e1);

      x1 = x0 - cg/5;
      //printf("Tj1 %3.12f\n",Tj1);

      if (ABS(x0) > 2*Amax/Jmax ) {
	x1 = (Amaxdc-A1dc);
      }
      if (isnan(x1)) {
	x1 = (Amaxdc-A1dc);
      }



      x0=x1;

      Tj1=x1;

      //printf("Tj1 %f\n",Tj1);
      if (i>150) { break;}
    } while (ABS(Tj1_0 -Tj1) > epsilon);
    //printf("Convergence de Newton en %d iterations\n",i);
    //printf("Tj1 %f\n",Tj1);
    Tj2 = ( Af - sqrt( (2 * Af * Af - 4 * Vf * Jmax + 4 * Jmax * Jmax * Tj1 * Tj1 + 4 * V1dc * Jmax + 8 * A1dc * Jmax * Tj1 + 2 * A1dc * A1dc)) / 0.2e1) /  Jmax;

    //printf("Tj2 %f\n",Tj2);
    Tjpa = Tj1+Tjp1;
    Taca = 0;
    Tjna = Tj1+Tj2+Tjdc;
    Tjnb = 0;
    Tacb = 0;
    Tjpb = Tj2;
    Tvc = 0;
  }


  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_VerifyTimesType2(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_TIMES* Time, double epsilon)
{
  double Tjpa, Tjpa2, Tjpa3;
  double Taca, Taca2;
  double Tjna, Tjna2, Tjna3;
  double Tvc;
  double Tjnb, Tjnb2, Tjnb3;
  double Tacb, Tacb2;
  double Tjpb, Tjpb2, Tjpb3;
  double Jmax, Amax, Vmax;
  double A0, V0, X0, Xf, Af, Vf;
  double A1, V1, X1, A2, V2, X2, A3, V3, X3, A4, V4, X4,A5, V5, X5, A6, V6, X6;
  double Af_cc, Vf_cc, Xf_cc;
  //  double epsilon = 1e-04;
  double epsilon_times = 1e-08;

  /* Set Times */
  Tjpa = Time->Tjpa;
  Taca = Time->Taca;
  Tjna = Time->Tjna;
  Tvc  = Time->Tvc;
  Tjnb = Time->Tjnb;
  Tacb = Time->Tacb;
  Tjpb = Time->Tjpb;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = ICm->x;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  Tjpa2 = Tjpa*Tjpa;
  Tjpa3 = Tjpa*Tjpa2;
  Taca2 = Taca*Taca;
  Tjna2 = Tjna*Tjna;
  Tjna3 = Tjna*Tjna2;
  Tjnb2 = Tjnb*Tjnb;
  Tjnb3 = Tjnb*Tjnb2;
  Tacb2 = Tacb*Tacb;
  Tjpb2 = Tjpb*Tjpb;
  Tjpb3 = Tjpb*Tjpb2;

  /*   printf("Tjna %f\n",Tjna); */
  /*   printf("Taca %f\n",Taca); */
  /*   printf("Tjpa %f\n",Tjpa); */
  /*   printf("Tvc %f\n",Tvc); */
  /*  printf("Tjpb %f\n",Tjpb); */
  /*   printf("Tacb %f\n",Tacb); */
  /*   printf("Tjnb %f\n",Tjnb); */


  if (ABS(Tjpa) < epsilon_times) { Tjpa = 0.0;}
  if (ABS(Taca) < epsilon_times) { Taca = 0.0;}
  if (ABS(Tjna) < epsilon_times) { Tjna = 0.0;}
  if (ABS(Tvc) < epsilon_times) { Tvc = 0.0;}
  if (ABS(Tjnb) < epsilon_times) { Tjnb = 0.0;}
  if (ABS(Tacb) < epsilon_times) { Tacb = 0.0;}
  if (ABS(Tjpb) < epsilon_times) { Tjpb = 0.0;}

  if (Tjpa<0) {
    //printf("Times incorrect\n");
    //printf("Tjpa<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb) */;
    //    return SM_ERROR;
  }
  if (Taca<0) {
    //printf("Times incorrect\n");
    //printf("Taca<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb); */
    //    return SM_ERROR;
  }

  if (Tjna<0) {
    //printf("Times incorrect\n");
    //printf("Tjna<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb); */



    //    return SM_ERROR;
  }

  if (Tvc<0) {
    //printf("Times incorrect\n");
    //printf("Tvc<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb); */
    //    return SM_ERROR;
  }

  if (Tjnb<0) {
    //printf("Times incorrect\n");
    //printf("Tjnb<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb); */
    //   return SM_ERROR;
  }

  if (Tacb<0) {
    //printf("Times incorrect\n");
    //printf("Tacb<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb );*/
    //   return SM_ERROR;
  }

  if (Tjpb<0) {
    //printf("Times incorrect\n");
    //printf("Tjpb<0\n");
    /*     printf("Tjna %f\n",Tjna); */
    /*     printf("Taca %f\n",Taca); */
    /*     printf("Tjpa %f\n",Tjpa); */
    /*     printf("Tvc %f\n",Tvc); */
    /*     printf("Tjpb %f\n",Tjpb); */
    /*     printf("Tacb %f\n",Tacb); */
    /*     printf("Tjnb %f\n",Tjnb) */;
    //    return SM_ERROR;
  }

  A1 = A0 - Jmax*Tjna;
  V1 = V0 + A0*Tjna - Jmax*Tjna2/2;
  X1 = X0 + V0*Tjna + A0*Tjna2/2 - Jmax*Tjna3/6;
  A2 = A1;
  V2 = V1 + A1*Taca;
  X2 = X1 + V1*Taca + A1*Taca2/2;
  A3 = A2 + Jmax*Tjpa;
  V3 = V2 + A2*Tjpa + Jmax*Tjpa2/2;
  X3 = X2 + V2*Tjpa + A2*Tjpa2/2 + Jmax*Tjpa3/6;
  A4  = A3;
  V4  = V3;
  X4  = X3 + V3*Tvc;
  A5 = A4  +  Jmax*Tjpb;
  V5 = V4  +  A4*Tjpb +  Jmax*Tjpb2/2;
  X5 = X4  +  V4*Tjpb +  A4*Tjpb2/2 + Jmax*Tjpb3/6;
  A6 = A5;
  V6 = V5 + A5*Tacb;
  X6 = X5 + V5*Tacb + A5*Tacb2/2;
  Af_cc = A6 - Jmax*Tjnb;
  Vf_cc = V6 + A6*Tjnb - Jmax*Tjnb2/2;
  Xf_cc = X6 + V6*Tjnb + A6*Tjnb2/2 - Jmax*Tjnb3/6;




  if ((ABS(Xf-Xf_cc)<epsilon) && (ABS(Vf-Vf_cc)<epsilon) && (ABS(Af-Af_cc)<epsilon)) {
    //printf("    ******************************\n");
    //printf("    |     Times are correct      |\n");
    //printf("    ******************************\n");

  }
  else {
    printf("Times incorrect\n");
    printf("Tjna %f\n",Tjna);
    printf("Taca %f\n",Taca);
    printf("Tjpa %f\n",Tjpa);
    printf("Tvc %f\n",Tvc);
    printf("Tjpb %f\n",Tjpb);
    printf("Tacb %f\n",Tacb);
    printf("Tjnb %f\n",Tjnb);
    printf("ICm.a %f\n",ICm->a);
    printf("ICm.v %f\n",ICm->v);
    printf("ICm.x %f\n",ICm->x);
    printf("FCm.a %f\n",FCm->a);
    printf("FCm.v %f\n",FCm->v);
    printf("FCm.x %f\n",FCm->x);
    printf("Erreur Position %f\n", ABS(Xf-Xf_cc));
    printf("Erreur Vitesse %f\n", ABS(Vf-Vf_cc));
    printf("Erreur Acceleration %f\n", ABS(Af-Af_cc)); 
    return SM_ERROR;
  }
  return SM_OK;
}

SM_STATUS sm_Jerk_Profile_dc(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm, SM_PARTICULAR_VELOCITY* PartVel, int *zone, SM_TIMES* Time)
{

  //function [Tjpa,Taca,Tjna,Tvc,Tjnb,Tacb,Tjpb]=sm_JerkProfile_dc(Amax,Vmax,Jmax,A0,V0,Af,Vf,zone,Vfmm,Vfmp,Vsmp,Vsmm)
  double A0, V0, X0, Xf, Af, Vf;
  double A1, V1;
  double Jmax, Amax, Vmax;
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb, Tjn;


  /* Set Times */
  Tjpa = Time->Tjpa;
  Taca = Time->Taca;
  Tjna = Time->Tjna;
  Tvc  = Time->Tvc;
  Tjnb = Time->Tjnb;
  Tacb = Time->Tacb;
  Tjpb = Time->Tjpb;

  /* Set Limits */
  Jmax  = limitsGoto->maxJerk;
  Amax  = limitsGoto->maxAcc;
  Vmax  = limitsGoto->maxVel;
  /* Set IC and FC */
  V0 = ICm->v;
  A0 = ICm->a;
  X0 = ICm->x;
  Vf = FCm->v;
  Af = FCm->a;
  Xf = FCm->x;

  if (*zone == 0) {
    if (A0>0) {
      A1 = -A0;
      V1 = V0;
      Tjpa = 0;
      Taca = 0;
      Tjna = 0;
      Tvc  = 0;
      Tjnb = (A0-A1) /Jmax;
      Tacb = 0;
      Tjpb = Tjnb;
    }
    if (A0<0) {
      A1 = -A0;
      V1 = V0;
      Tjpa = (A1-A0) /Jmax;
      Taca = 0;
      Tjna = Tjpa;
      Tvc  = 0;
      Tjnb = 0;
      Tacb = 0;
      Tjpb = 0;
    }
    if (A0 == 0) {
      Tjpa = 0;
      Taca = 0;
      Tjna = 0;
      Tvc  = 0;
      Tjnb = 0;
      Tacb = 0;
      Tjpb = 0;
    }
  }
  else if (*zone == 1) {
    Tjpa = (Amax - A0) / Jmax ;
    Taca = (PartVel->Vfmm - PartVel->Vsmp) / Amax ;
    Tjn = (Amax - Af) / Jmax;
    if (Tjn >= Amax/Jmax) {
      Tjna = Amax/Jmax;
      Tjnb = Tjn-Tjna;
    }
    else {
      Tjnb=0;
      Tjna=Tjn;
    }
    Tvc = 0;
    Tacb = 0;
    Tjpb = 0;
  }
  else if (*zone == 2) {
    Tjpa = 0;
    Taca = 0;
    Tvc = 0;
    Tjn = (A0 + Amax) / Jmax;
    Tacb = -(PartVel->Vfmp - PartVel->Vsmm) / Amax;
    Tjpb = (Af + Amax) / Jmax;

    if (Tjn >= Amax/Jmax) {
      Tjnb = Amax/Jmax;
      Tjna = Tjn-Tjnb;
    }
    else {
      Tjna=0;
      Tjnb=Tjn;
    }
  }
  else if (*zone == 3) {
    A1 = sqrt( Jmax*(Vf - V0) + (0.5)*(A0*A0 + Af*Af));
    V1 = V0 + (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = (A1 - A0) / Jmax ;
    Taca = 0;
    Tjna = (A1 - Af) / Jmax;
    Tvc  = 0;
    Tjnb = 0;
    Tacb = 0;
    Tjpb = 0;
  }
  else if (*zone == 4) {
    A1 = - sqrt( Jmax*(V0 - Vf) + (0.5)*(A0*A0 + Af*Af));
    V1 = V0 - (1/(2*Jmax))*(A1*A1 - A0*A0);
    Tjpa = 0;
    Taca = 0;
    Tjna = 0;
    Tvc  = 0;
    Tjnb = (A0 - A1) / Jmax;
    Tacb = 0;
    Tjpb = (Af - A1) / Jmax ;
  }
  else if (*zone == 5) {
    Tjpa = (Af - A0) / Jmax ;
    Taca = 0;
    Tjna = 0;
    Tvc  = 0;
    Tjnb = 0;
    Tacb = 0;
    Tjpb = 0;
  }
  else if (*zone == 6) {
    Tjpa = 0;
    Taca = 0;
    Tjna = 0;
    Tvc  = 0;
    Tjnb = (A0 - Af) / Jmax;
    Tacb = 0;
    Tjpb = 0;
  }
  Time->Tjpa = Tjpa;
  Time->Taca = Taca;
  Time->Tjna = Tjna;
  Time->Tvc  = Tvc;
  Time->Tjnb = Tjnb;
  Time->Tacb = Tacb;
  Time->Tjpb = Tjpb;

  return SM_OK;
}

SM_STATUS sm_Jerk_Profile_Type_1(SM_LIMITS* limitsGoto, SM_COND* ICm, SM_COND* FCm,SM_PARTICULAR_VELOCITY* PartVel, double* dc, int* zone, SM_TIMES* Time)
{
  double DSVmax, DSAmax, DSAmaxT12_31, DSAmaxT12_32, DSAmaxT12_33, DSAmaxT12_2, DSAmaxT12_22, DSAmaxT12_4, DSAmaxT12_311;
  double Amax, Vmax;
  double Jmax;
  double Vswitch;
  Amax = limitsGoto->maxAcc ;
  Vmax = limitsGoto->maxVel;
  Jmax =  limitsGoto->maxJerk;

  sm_CalculOfDSVmaxType1(limitsGoto, PartVel, ICm, FCm, &DSVmax);
  sm_CalculOfDSAmaxType1(limitsGoto, PartVel, ICm, FCm, &DSAmax);
  //printf("DSVmaxT1 : %f\n",DSVmax);
  //printf("DSAmaxT1 : %f\n",DSAmax);

  //  ICm->a = (double)((int)(ICm->a * 1e10))/1e10;
  //  ICm->v = (double)((int)(ICm->v * 1e10))/1e10;
  //
  //  FCm->a = (double)((int)(FCm->a * 1e10))/1e10;
  //  FCm->v = (double)((int)(FCm->v * 1e10))/1e10;
  //  FCm->x = (double)((int)(FCm->x * 1e10))/1e10;

  if (FCm->x >= DSVmax) {

    if (PartVel->Vf0m <= PartVel->Vlim) {

      if ((FCm->a > 0) || ((FCm->a <= 0 ) && (FCm->a > -Amax) && (PartVel->Vf0p < Vmax))) {
	//	printf("1 Chemin type 1 avec Vmax et -Amax atteind\n");
	sm_JerkProfile_Type1_VmaxAmin_7seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);
      }
      else if ((FCm->a < 0) && (PartVel->Vf0p == Vmax)) {
	//	printf("2 Chemin type 1 avec Vmax et -Amax atteind\n");
	sm_JerkProfile_Type1_VmaxAmin_5seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);
      }
      else if (FCm->a == -Amax) {
	//	printf("3 Chemin type 1 avec Vmax et -Amax atteind\n");
	sm_JerkProfile_Type1_VmaxAmin_6seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);
      }
    }
    else if (PartVel->Vf0m > PartVel->Vlim) {
      if (PartVel->Vf0p == Vmax) {
	if (FCm->a < 0) {

	  //	  printf("1-1 Chemin type 1 avec Vmax atteind et -Amax non atteind\n");
	  sm_JerkProfile_Type1_Vmax_5seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);

	}
	else {
	  // printf("2 Chemin type 1 avec Vmax atteind et -Amax non atteind\n");

	  sm_JerkProfile_Type1_VmaxAmin_7seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);
	}
      }
      else {
	//	printf("3 Chemin type 1 avec Vmax atteind et -Amax non atteind\n");

	sm_JerkProfile_Type1_Vmax_6seg_maxi(limitsGoto, ICm, FCm, PartVel, Time);
      }
    }
  }
  else if (FCm->x >= DSAmax) {
    if ((PartVel->Vf0m <= PartVel->Vlim) &&  (PartVel->Vs0m <= PartVel->Vlim)) {
      //  printf("4 Chemin type 1 avec Amax et Amin atteind et Vmax non atteind\n");
      sm_JerkProfile_Type1_VmaxAmin_4seg(limitsGoto, ICm, FCm, PartVel, Time);
    }
  }
  else if (FCm->x >= *dc)  {

    /*c'est les cas les plus complexes car on doit modifier les paraboles sans atteindre DSAmax
      Ces modifications changes en fonction du type de zone determinee au depart */
    if (*zone == 0) {
      sm_JerkProfile_Type1_inf_DSAmax_Z0(limitsGoto, ICm, FCm, Time);
    }
    else if (*zone == 1) {


      if ((FCm->a <= 0)||(isnan(DSAmaxT12_31))) {
	sm_JerkProfile_Type1_inf_DSAmax_Z1(limitsGoto, ICm, FCm, PartVel, Time, dc);
      }
      else {

	Vswitch = PartVel->Vsmp + (1/(2*Jmax))*(Amax*Amax);
	sm_Calcul_Of_DSAmax2_31_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_31);
	//	printf("DSAMAX_12_31 %f\n",DSAmaxT12_31);
	if (PartVel->Vf0m > Vswitch) {


	  if (( FCm->x >= DSAmaxT12_31) || isnan(DSAmaxT12_31)) {
	    //  printf("Z1\n");
	    sm_JerkProfile_Type1_inf_DSAmax_Z1(limitsGoto, ICm, FCm, PartVel, Time, dc);
	  }
	  else {
	    // printf("Z2\n");
	    sm_JerkProfile_Type1_inf_DSAmax_inf_DSZ1_Z1(limitsGoto, ICm, FCm, PartVel, Time);
	  }
	}

	else {
	  sm_Calcul_Of_DSAmax2_311_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_311);
	  sm_Calcul_Of_DSAmax2_33_Type1(limitsGoto, ICm, FCm, &DSAmaxT12_33);

	  if (( FCm->x >= DSAmaxT12_31)) {
	    //	  printf("Z3\n");
	    sm_JerkProfile_Type1_inf_DSAmax_Z1(limitsGoto, ICm, FCm, PartVel, Time, dc);
	  }
	  else if (( FCm->x >= DSAmaxT12_33 )||((PartVel->Vs0p >PartVel->Vf0m)&&ICm->a >0))  {
	    sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	  }
	  else if ( FCm->x >= DSAmaxT12_311 ) {
	    //  printf("Z4\n");
	    sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_33_Z3(limitsGoto, ICm, FCm,  Time);

	  }
	  else {
	    //  printf("Z5\n");
	    sm_JerkProfile_Type1_inf_DSAmax_inf_DSZ1_Z1(limitsGoto, ICm, FCm, PartVel, Time);
	  }
	}
      }
    }

    else if (*zone == 2) {

      if (ICm->a >=0) {
	sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
      }
      else {

	Vswitch = PartVel->Vs0m - (1/(2*Jmax))*(Amax*Amax);
	if (isnan(DSAmaxT12_2)) {
	  sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	}
	else {
	  if (PartVel->Vfmp > Vswitch) {
	    sm_Calcul_Of_DSAmax2_2_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_2);
	    sm_Calcul_Of_DSAmax2_22_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_22);
	    if ((PartVel->Vs0m < PartVel->Vf0p) && (FCm->a <0) && (ICm->a <0)) {
	      if (FCm->x >= DSAmaxT12_2) {
		sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	      }
	      else {
		sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	      }
	    }
	    else {
	      if (FCm->x >= DSAmaxT12_2) {
		//printf("9) Vmax et DSAmaxT1 non atteind Zone 2\n");
		sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	      }
	      // else {
	      else if (FCm->x >= DSAmaxT12_22) {
		sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	      }
	      else {
		sm_JerkProfile_Type1_inf_DSAmax_inf_DSAmaxT12_22_Z2(limitsGoto, ICm, FCm, PartVel, Time);
		if (Time->Tacb <0) {
		  sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
		}

	      }
	    }
	  }
	  else {
	    sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	  }
	}
      }


    }
    else if (*zone == 3) {

      if(PartVel->Vfmp >= PartVel->Vsmp) {
	//	printf("FCm.a %f",FCm->a);
	if (FCm->a > 0) {
	  sm_Calcul_Of_DSAmax2_31_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_31);
	  //  printf("DSAmaxT12_31 %f\n",DSAmaxT12_31);
	  sm_Calcul_Of_DSAmax2_33_Type1(limitsGoto, ICm, FCm, &DSAmaxT12_33);
	  //  printf("DSAmaxT12_33 %f\n",DSAmaxT12_33);
	  if ((FCm->x > DSAmaxT12_33)|| ((PartVel->Vs0p > PartVel->Vf0m) && ((FCm->a) >0) && ((ICm->a) >0))) {
	    if (FCm->x > DSAmaxT12_31) {

	      sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_31_Z3(limitsGoto, ICm, FCm, PartVel, Time);
	    }
	    else {
	      sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	    }
	  }
	  else {
	    sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_33_Z3(limitsGoto, ICm, FCm,  Time);
	  }
	}
	else {
	  sm_Calcul_Of_DSAmax2_31_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_31);

	  if (FCm->x > DSAmaxT12_31) {
	    sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_31_Z3(limitsGoto, ICm, FCm, PartVel, Time);
	  }
	  else {
	    sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	  }
	}
      }

      else {
	sm_Calcul_Of_DSAmax2_32_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_32);

	if (FCm->x > DSAmaxT12_32) {
	  sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	}
	else {
	  sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	}
      }
    }
    else if (*zone == 4) {

      if( PartVel->Vsmp <  PartVel->Vfmp) {
	//printf("Sm nouveau bug A1 is Amax before A5\n");
	sm_Calcul_Of_DSAmax2_31_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_31);
	if(FCm->x >= DSAmaxT12_31) {
	  //printf("Amax atteind pour A1\n");
	  sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_31_Z3(limitsGoto, ICm, FCm, PartVel, Time);
	} else {
	  sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	}

      } else {
	sm_Calcul_Of_DSAmax2_4_Type1(limitsGoto,ICm,  FCm,  PartVel, dc, &DSAmaxT12_4);
	//  printf("DSAmaxT12_4 %f\n",DSAmaxT12_4);
	
	if (FCm->x > DSAmaxT12_4) {
	  sm_JerkProfile_Type1_inf_DSAmax_Z2(limitsGoto, ICm, FCm, PartVel, Time, dc);
	}
	else {
	  
	  sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
	}
      } // fin bug else ici
    }
    else if (*zone == 5) {

      sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
    }
    else if (*zone == 6) {

      sm_Calcul_Of_DSAmax2_32_Type1(limitsGoto, ICm, FCm, PartVel, &DSAmaxT12_32);
      //printf("DSAmaxT12_32 %f\n",DSAmaxT12_32);
      if (FCm->x > DSAmaxT12_32) {
	sm_JerkProfile_Type1_inf_DSAmax_sup_DSAmax2_32_Z3(limitsGoto, ICm, FCm, PartVel, Time);
      }
      else {
	sm_JerkProfile_Type1_inf_DSAmax_Z3(limitsGoto, ICm, FCm, Time);
      }
    }
  }
  return SM_OK;
}

/*
 * This is the main function of the library to compute
 * softMotion.
 */
SM_STATUS sm_ComputeSoftMotionLocal(SM_COND IC_e, SM_COND FC_e, SM_LIMITS limitsGoto, SM_TIMES *T_Jerk, int *TrajectoryType, double* dcOut, int* zoneOut)
{
  SM_COND IC, FC, ICm, FCm, ICmInv, FCmInv;
  SM_PARTICULAR_VELOCITY PartVel,PartVelInv;
  double dc=0, dcInv=0;
  int zone=-1, zoneInv;
  SM_TIMES Time, Tinv;
  double Amax, Vmax, Jmax;
  double epsilon_dc = 0.001;
  double epsilon_erreur;

  Amax = limitsGoto.maxAcc ;
  Vmax = limitsGoto.maxVel;
  Jmax = limitsGoto.maxJerk;

  Time.Tjpa =0.0;
  Time.Taca =0.0;
  Time.Tjna =0.0;
  Time.Tvc  =0.0;
  Time.Tjnb =0.0;
  Time.Tacb =0.0;
  Time.Tjpb =0.0;

  IC.a = TRUNC(IC_e.a);
  IC.v = TRUNC(IC_e.v);
  IC.x = TRUNC(IC_e.x);
  FC.a = TRUNC(FC_e.a);
  FC.v = TRUNC(FC_e.v);
  FC.x = TRUNC(FC_e.x);

  /* Check if IC and FC are inside the valid acceleration/velocity frame and 
     if not, IC and FC are projeted to be valid
     then compute the particular velocities */
  sm_VerifyInitialAndFinalConditions(&limitsGoto, &IC, &FC, &PartVel, &ICm, &FCm);

  /* Compute the critical length and the zone */
  sm_CalculOfCriticalLengthLocal(&limitsGoto, &PartVel, &ICm, &FCm, &dc, &zone);
  *dcOut = dc;
  *zoneOut = zone;

  if (ABS(FCm.x - dc) < epsilon_dc) {
    // The motion is the critical motion
    sm_Jerk_Profile_dc(&limitsGoto, &ICm, &FCm, &PartVel, &zone, &Time);
    *dcOut = dc;
    *zoneOut = zone;
    //    epsilon_erreur = 0.001;
    //    if (sm_VerifyTimesType1(&limitsGoto, &ICm, &FCm, &Time, epsilon_erreur)!=0) {
    //      return SM_ERROR;
    //    }
    *TrajectoryType = 1;
    T_Jerk->Tjpa =  ABS(Time.Tjpa);
    T_Jerk->Taca =  ABS(Time.Taca);
    T_Jerk->Tjna =  ABS(Time.Tjna);
    T_Jerk->Tvc  =  ABS(Time.Tvc) ;
    T_Jerk->Tjnb =  ABS(Time.Tjnb);
    T_Jerk->Tacb =  ABS(Time.Tacb);
    T_Jerk->Tjpb =  ABS(Time.Tjpb);
  }
  else {
    epsilon_erreur = 1e-04;
    if ( FCm.x > dc) {
      //     printf("Xf > Critical length => Trajectory type:1\n");
      *TrajectoryType = 1;
      if( sm_Jerk_Profile_Type_1(&limitsGoto, &ICm, &FCm, &PartVel, &dc, &zone, &Time)!=0) {
	return  SM_ERROR;
      }

      if (sm_VerifyTimesType1(&limitsGoto, &ICm, &FCm, &Time, epsilon_erreur)!=0) {
	//	printf("ERROR JerkProfile Type 1\n");
	//	printf("IC.a %f\n",IC.a);
	//	printf("IC.v %f\n",IC.v);
	//	printf("IC.x %f\n",IC.x);
	//	printf("FC.a %f\n",FC.a);
	//	printf("FC.v %f\n",FC.v);
	//	printf("FC.x %f\n",FC.x);
	//	printf("Jmax %f\n",Jmax);
	//	printf("Amax %f\n",Amax);
	//	printf("Vmax %f\n",Vmax);
	//	printf("Zone %d\n",zone);
	//	printf("dc %f\n",dc);
	//	printf("TrajectoryType %d\n", *TrajectoryType);
	//	printf("T_Jerk dans sm_JerkProfile\n");
	//	printf("T_Jerk.Tjpa %f\n", T_Jerk->Tjpa);
	//	printf("T_Jerk.Taca %f\n", T_Jerk->Taca);
	//	printf("T_Jerk.Tjna %f\n", T_Jerk->Tjna);
	//	printf("T_Jerk.Tvc %f\n", T_Jerk->Tvc);
	//	printf("T_Jerk.Tjnb %f\n", T_Jerk->Tjnb);
	//	printf("T_Jerk.Tacb %f\n", T_Jerk->Tacb);
	//	printf("T_Jerk.Tjpb %f\n", T_Jerk->Tjpb);
	//	printf("\n");
	return SM_ERROR;
      }
      T_Jerk->Tjpa =  ABS(Time.Tjpa);
      T_Jerk->Taca =  ABS(Time.Taca);
      T_Jerk->Tjna =  ABS(Time.Tjna);
      T_Jerk->Tvc  =  ABS(Time.Tvc) ;
      T_Jerk->Tjnb =  ABS(Time.Tjnb);
      T_Jerk->Tacb =  ABS(Time.Tacb);
      T_Jerk->Tjpb =  ABS(Time.Tjpb);
    }
    else {
      //    printf("Xf < Critical length => Trajectory type:2\n");
      *TrajectoryType = -1;
      // inverse the initial and final conditions, dc, and zones
      ICmInv.a = -ICm.a;
      ICmInv.v = -ICm.v;
      ICmInv.x = -ICm.x;
      FCmInv.a = -FCm.a;
      FCmInv.v = -FCm.v;
      FCmInv.x = -FCm.x;
      dcInv = -dc;
      if (zone==0) {zoneInv =0;}
      if (zone==1) {zoneInv =2;}
      if (zone==2) {zoneInv =1;}
      if (zone==3) {zoneInv =4;}
      if (zone==4) {zoneInv =3;}
      if (zone==5) {zoneInv =6;}
      if (zone==6) {zoneInv =5;}

      sm_VerifyInitialAndFinalConditions(&limitsGoto, &ICmInv, &FCmInv, &PartVelInv, &ICmInv, &FCmInv);

      if( sm_Jerk_Profile_Type_1(&limitsGoto, &ICmInv, &FCmInv, &PartVelInv, &dcInv, &zoneInv, &Tinv)!=0) {
	return SM_ERROR;
      }
      Time.Tjna = Tinv.Tjpa;
      Time.Taca = Tinv.Taca;
      Time.Tjpa = Tinv.Tjna;
      Time.Tvc  = Tinv.Tvc;
      Time.Tjpb = Tinv.Tjnb;
      Time.Tacb = Tinv.Tacb;
      Time.Tjnb = Tinv.Tjpb;

      // sort the seven times as a type 2 motion
      T_Jerk->Tjpa = ABS(Time.Tjna);
      T_Jerk->Taca = ABS(Time.Taca);
      T_Jerk->Tjna = ABS(Time.Tjpa);
      T_Jerk->Tvc  = ABS(Time.Tvc) ;
      T_Jerk->Tjnb = ABS(Time.Tjpb);
      T_Jerk->Tacb = ABS(Time.Tacb);
      T_Jerk->Tjpb = ABS(Time.Tjnb);

      if (sm_VerifyTimesType2(&limitsGoto, &ICm, &FCm, &Time, epsilon_erreur)!=0) {
	//	printf("ERROR JerkProfile Type 2\n");
	//	printf("IC.a %f\n",IC.a);
	//	printf("IC.v %f\n",IC.v);
	//	printf("IC.x %f\n",IC.x);
	//	printf("FC.a %f\n",FC.a);
	//	printf("FC.v %f\n",FC.v);
	//	printf("FC.x %f\n",FC.x);
	//	printf("Jmax %f\n",Jmax);
	//	printf("Amax %f\n",Amax);
	//	printf("Vmax %f\n",Vmax);
	//	printf("Zone %d\n",zone);
	//	printf("dc %f\n",dc);
	//	printf("TrajectoryType %d\n", *TrajectoryType);
	//	printf("T_Jerk dans sm_JerkProfile\n");
	//	printf("T_Jerk.Tjpa %f\n", T_Jerk->Tjpa);
	//	printf("T_Jerk.Taca %f\n", T_Jerk->Taca);
	//	printf("T_Jerk.Tjna %f\n", T_Jerk->Tjna);
	//	printf("T_Jerk.Tvc %f\n", T_Jerk->Tvc);
	//	printf("T_Jerk.Tjnb %f\n", T_Jerk->Tjnb);
	//	printf("T_Jerk.Tacb %f\n", T_Jerk->Tacb);
	//	printf("T_Jerk.Tjpb %f\n", T_Jerk->Tjpb);
	//	printf("\n");
	return SM_ERROR;
      }
    }
  }
  return SM_OK;
}

SM_STATUS sm_ComputeSoftMotion(SM_COND IC, SM_COND FC, SM_LIMITS limitsGoto, SM_TIMES *T_Jerk, int *TrajectoryType)
{
  double dcOut;
  int zoneOut;
  if(sm_ComputeSoftMotionLocal( IC, FC, limitsGoto, T_Jerk, TrajectoryType, &dcOut, &zoneOut)!=0) {
    return SM_ERROR;
  }
  return SM_OK;
}

SM_STATUS sm_CalculOfCriticalLength(SM_COND IC, SM_COND FC, SM_LIMITS limitsGoto, double* dc)
{
  SM_PARTICULAR_VELOCITY PartVel;
  int zone=-1;
  SM_COND ICm, FCm;
  sm_VerifyInitialAndFinalConditions(&limitsGoto, &IC, &FC, &PartVel, &ICm, &FCm);
  sm_CalculOfCriticalLengthLocal(&limitsGoto, &PartVel, &ICm, &FCm, dc, &zone);
  //   printf("Zone : %d\n",zone);
  //   printf("Critical length : %f\n",*dc);

  return SM_OK;
}

SM_STATUS sm_AdjustTimeSlowingJerk(SM_COND IC, SM_COND FC,double MaxTime, SM_LIMITS Limits, SM_TIMES *Time, double* Jerk, SM_LIMITS* newLimits, int* DirTransition)
{
  SM_LIMITS auxLimits;
  double J = 0.0;
  double delay = 400;
  int TrajectoryType = 0;
  int i = 0;
  double TotalTime = 0.0;
  double a = 0;

  J =  Limits.maxJerk;
  auxLimits.maxAcc  = Limits.maxAcc;
  auxLimits.maxVel  = Limits.maxVel;

  /*   printf("IC.a %f\n",IC.a); */
  /*   printf("IC.v %f\n",IC.v); */
  /*   printf("IC.x %f\n",IC.x); */
  /*   printf("FC.a %f\n",FC.a); */
  /*   printf("FC.v %f\n",FC.v); */
  /*   printf("FC.x %f\n",FC.x); */

  i = 0;
  a = 0.005;
  do {
    J = J - a;
    auxLimits.maxJerk = J;
    auxLimits.maxAcc =  auxLimits.maxAcc - (1.0/3.0)*a;
    auxLimits.maxAcc = J/3.0;
    if (sm_ComputeSoftMotion(IC, FC, auxLimits, Time, &TrajectoryType)!=0) {
      //  printf("ERROR Jerk Profile Transition1\n");
      /*   J = J + a; */
      /*       a = a/2; */
      /*       auxLimits2.maxJerk = J; */
      /*       if (sm_JerkProfile(IC, FC, auxLimits2, T, &TrajectoryType)!=0) { */
      /* 	printf("ERROR Jerk Profile Transition1\n"); */
      /*       } */
    }
    TotalTime = (Time->Tjpa + Time->Taca + Time->Tjna + Time->Tvc + Time->Tjnb + Time->Tacb + Time->Tjpb);
    if   (TotalTime > MaxTime) {
      J = J + a;
      a = a/2;
    }
    i++;
    if (i> delay) {
      //       printf("ERROR Jerk Profile Adjusting Time is too long\n");
      //  printf("J = %f\n",J);
      // printf("MaxTime %f\n",MaxTime);
      return SM_ERROR;
    }
  } while (( ABS(MaxTime-TotalTime) > 0.001 )&& (J>0));

  //   printf("iteration %d\n",i);
  //   printf("J = %f\n",J);
  *Jerk = J;
  *DirTransition = TrajectoryType;
  newLimits->maxJerk = J;
  newLimits->maxAcc = auxLimits.maxAcc;
  newLimits->maxVel = auxLimits.maxVel;
  //   printf("Temps apres ajustement\n");
  //   printf("Tjpa %f\n",T->Tjpa);
  //   printf("Taca %f\n",T->Taca);
  //   printf("Tjna %f\n",T->Tjna);
  //   printf("Tvc %f\n",T->Tvc);
  //   printf("Tjnb %f\n",T->Tjnb);
  //   printf("Tacb %f\n",T->Tacb);
  //   printf("Tjpb %f\n",T->Tjpb);
  return SM_OK;
}


SM_STATUS sm_CalculTimeProfileWithVcFixed(double V0, double Vf, double Vc, SM_LIMITS Limits, SM_TIMES_GLOBAL * Time, int* dir_a, int* dir_b)
{
  /*
    V0 is the initial velocity
    Vf the final velocity
    A0 and Af are zero
    Vc is the velocity of the Tvc segment
    dir_a is the direction of the three first segments
    dir_b is the direction of the three last segments
  */

  double Jmax = Limits.maxJerk;
  double Amax = Limits.maxAcc;
  //double Vmax = Limits.maxVel;
  double A1, V1, V2;

  /* Calcul of the three first segments */
  if (V0 > Vc) {
    *dir_a = -1;
    if ( ( V0- Vc) > ((Amax*Amax)/Jmax) ) {
      // Amin is reached
      Time->T1 = Amax/Jmax;
      V1 = V0 - (1.0/(2.0*Jmax))*Amax*Amax;
      V2 = Vc + (1.0/(2.0*Jmax))*Amax*Amax;
      Time->T3 = Time->T1;
      Time->T2 = (V1 - V2) / Amax;
    } else {
      //Amin isn't reached
      A1 = - sqrt(Jmax*(V0-Vc));
      V1 = V0 - (1.0/(2.0*Jmax))*A1*A1;
      Time->T1 = - A1 / Jmax;
      Time->T3 = Time->T1;
      Time->T2 = 0.0;
    }
  }
  else if (V0 < Vc) {
    *dir_a = 1;
    if ((Vc - V0) > ((Amax*Amax) / Jmax)) {
      // Amax is reached
      Time->T1 = Amax/Jmax;
      V1 = V0 + (1.0/(2.0*Jmax))*Amax*Amax;
      V2 = Vc - (1.0/(2.0*Jmax))*Amax*Amax;
      Time->T3 = Time->T1;
      Time->T2 = (V2 - V1) / Amax;
    } else {
      // Amax isn't reached
      A1 = sqrt(Jmax*(Vc-V0));
      V1 = V0 + (1.0/(2.0*Jmax))*A1*A1;
      Time->T1 = A1 / Jmax;
      Time->T3 = Time->T1;
      Time->T2 = 0.0;
    }
  }
  else if (V0 == Vc) {
    *dir_a = 1;
    Time->T1 = 0.0;
    Time->T2 = 0.0;
    Time->T3 = 0.0;
  }

  /* Calcul of the three last segments */
  if (Vf > Vc) {
    *dir_b = 1;
    if ((Vf - Vc) > ((Amax*Amax) / Jmax)) {
      // Amax is reached
      Time->T5 = Amax/Jmax;
      V1 = Vc + (1.0/(2.0*Jmax))*Amax*Amax;
      V2 = Vf - (1.0/(2.0*Jmax))*Amax*Amax;
      Time->T7 = Time->T5;
      Time->T6 = (V2 - V1) / Amax;
    } else {
      // Amax isn't reached
      A1 = sqrt(Jmax*(Vf-Vc));
      V1 = Vc + (1.0/(2.0*Jmax))*A1*A1;
      Time->T5 = A1 / Jmax;
      Time->T7 = Time->T5;
      Time->T6 = 0.0;
    }
  }

  else if (Vf < Vc) {
    *dir_b = -1;
    if ((Vc - Vf) > ((Amax*Amax) / Jmax)) {
      // Amin is reached
      Time->T5 = Amax/Jmax;
      V1 = Vc - (1.0/(2.0*Jmax))*Amax*Amax;
      V2 = Vf + (1.0/(2.0*Jmax))*Amax*Amax;
      Time->T7 = Time->T5;
      Time->T6 = (V1 - V2) / Amax;
    } else {
      // Amin isn't reached
      A1 = - sqrt(Jmax*(Vc-Vf));
      V1 = Vc - (1/(2*Jmax))*A1*A1;
      Time->T5 = - A1 / Jmax;
      Time->T7 = Time->T5;
      Time->T6 = 0.0;
    }
  }

  else if (Vf == Vc) {
    *dir_b = 1;
    Time->T5 = 0.0;
    Time->T6 = 0.0;
    Time->T7 = 0.0;
  }

  return SM_OK;
}


SM_STATUS sm_CalculTimeProfileWithVcFixedWithAcc(double V0, double Vf,double A0, double Af, double Vc, SM_LIMITS Limits, SM_TIMES_GLOBAL *Time, int* dir_a, int* dir_b)
{
  /*
    V0 is the initial velocity
    Vf the final velocity
    A0 and Af are zero
    Vc is the velocity of the Tvc segment
    dir_a is the direction of the three first segments
    dir_b is the direction of the three last segments
  */
  double Vs0p, Vs0m, Vf0p, Vf0m;
  double Jmax = Limits.maxJerk;
  double Amax = Limits.maxAcc;
  //double Vmax = Limits.maxVel;
  double TdiffA0, TdiffAf;
  double A1, V1, V2;

  Vs0p = V0 - (1/(2*Jmax))*(0 - A0*A0);
  Vs0m = V0 + (1/(2*Jmax))*(0 - A0*A0);
  Vf0p = Vf - (1/(2*Jmax))*(0 - Af*Af);
  Vf0m = Vf + (1/(2*Jmax))*(0 - Af*Af);
  TdiffA0 = ABS(A0)/Jmax;
  TdiffAf = ABS(Af)/Jmax;

  /* Calcul of the three first segments */
  if (A0 >=0) {
    if (Vc <= Vs0p) {

      *dir_a = -1;
      if ( ( Vs0p- Vc) > ((Amax*Amax)/Jmax) ) {
	// Amin is reached
	Time->T1 = Amax/Jmax + TdiffA0;
	V1 = Vs0p - (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vc + (1.0/(2*Jmax))*Amax*Amax;
	Time->T3 = Amax/Jmax;
	Time->T2 = (V1 - V2) / Amax;
      } else {
	//Amin isn't reached
	A1 = - sqrt(Jmax*(Vs0p-Vc));
	V1 = Vs0p - (1/(2*Jmax))*A1*A1;
	Time->T1 = - A1 / Jmax + TdiffA0;
	Time->T3 = - A1 / Jmax;
	Time->T2 = 0.0;
      }
    }
    else if (Vc > Vs0p) {
      *dir_a = 1;
      if ((Vc - Vs0m) > ((Amax*Amax) / Jmax)) {
	// Amax is reached
	Time->T1 = Amax/Jmax - TdiffA0;
	V1 = Vs0m + (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vc - (1.0/(2*Jmax))*Amax*Amax;
	Time->T3 =  Amax/Jmax;
	Time->T2 = (V2 - V1) / Amax;
      } else {
	// Amax isn't reached
	A1 = sqrt(Jmax*(Vc-Vs0m));
	V1 = Vs0m + (1.0/(2*Jmax))*A1*A1;
	Time->T1 = A1 / Jmax - TdiffA0;
	Time->T3 = A1 / Jmax;
	Time->T2 = 0.0;
      }
    }
  }
  if (A0 < 0) {
    if (Vc <= Vs0m) {

      *dir_a = -1;
      if ( ( Vs0p- Vc) > ((Amax*Amax)/Jmax) ) {
	// Amin is reached
	Time->T1 = Amax/Jmax - TdiffA0;
	V1 = Vs0p - (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vc + (1.0/(2*Jmax))*Amax*Amax;
	Time->T3 = Amax/Jmax;
	Time->T2 = (V1 - V2) / Amax;
      } else {
	//Amin isn't reached
	A1 = - sqrt(Jmax*(Vs0p-Vc));
	V1 = Vs0p - (1/(2*Jmax))*A1*A1;
	Time->T1 = - A1 / Jmax - TdiffA0;
	Time->T3 = - A1 / Jmax;
	Time->T2 = 0.0;
      }
    }
    else if (Vc > Vs0m) {
      *dir_a = 1;
      if ((Vc - Vs0m) > ((Amax*Amax) / Jmax)) {
	// Amax is reached
	Time->T1 = Amax/Jmax + TdiffA0;
	V1 = Vs0m + (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vc - (1.0/(2*Jmax))*Amax*Amax;
	Time->T3 =Amax/Jmax;
	Time->T2 = (V2 - V1) / Amax;
      } else {
	// Amax isn't reached
	A1 = sqrt(Jmax*(Vc-Vs0m));
	V1 = Vs0m + (1.0/(2*Jmax))*A1*A1;
	Time->T1 = A1 / Jmax + TdiffA0;
	Time->T3 = A1 / Jmax;
	Time->T2 = 0.0;
      }
    }
  }


  /* Calcul of the three last segments */
  if (Af >= 0) {
    if (Vc <= Vf0m) {
      *dir_b = 1;
      if ((Vf0p - Vc) > ((Amax*Amax) / Jmax)) {
	// Amax is reached
	Time->T5 = Amax/Jmax;
	V1 = Vc + (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vf0p - (1.0/(2*Jmax))*Amax*Amax;
	Time->T7 =  Amax/Jmax - TdiffAf;
	Time->T6 = (V2 - V1) / Amax;
      } else {
	// Amax isn't reached
	A1 = sqrt(Jmax*(Vf0p-Vc));
	V1 = Vc + (1.0/(2*Jmax))*A1*A1;
	Time->T5 = A1 / Jmax;
	Time->T7 = A1 / Jmax - TdiffAf;
	Time->T6 = 0.0;
      }
    }

    else if (Vc > Vf0m) {
      *dir_b = -1;
      if ((Vc - Vf0m) > ((Amax*Amax) / Jmax)) {
	// Amin is reached
	Time->T5 = Amax/Jmax;
	V1 = Vc - (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vf0m + (1.0/(2*Jmax))*Amax*Amax;
	Time->T7 = Time->T5 + TdiffAf;
	Time->T6 = (V1 - V2) / Amax;
      } else {
	// Amin isn't reached
	A1 = - sqrt(Jmax*(Vc-Vf0m));
	V1 = Vc - (1/(2*Jmax))*A1*A1;
	Time->T5 = - A1 / Jmax;
	Time->T7 = Time->T5 + TdiffAf;
	Time->T6 = 0.0;
      }
    }
  }

  /* Calcul of the three last segments */
  if (Af < 0) {
    if (Vc <= Vf0p) {
      *dir_b = 1;
      if ((Vf0p - Vc) > ((Amax*Amax) / Jmax)) {
	// Amax is reached
	Time->T5 = Amax/Jmax;
	V1 = Vc + (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vf0p - (1.0/(2*Jmax))*Amax*Amax;
	Time->T7 =  Amax/Jmax + TdiffAf;
	Time->T6 = (V2 - V1) / Amax;
      } else {
	// Amax isn't reached
	A1 = sqrt(Jmax*(Vf0p-Vc));
	V1 = Vc + (1.0/(2*Jmax))*A1*A1;
	Time->T5 = A1 / Jmax;
	Time->T7 = A1 / Jmax + TdiffAf;
	Time->T6 = 0.0;
      }
    }

    else if (Vc > Vf0p) {
      *dir_b = -1;
      if ((Vc - Vf0m) > ((Amax*Amax) / Jmax)) {
	// Amin is reached
	Time->T5 = Amax/Jmax;
	V1 = Vc - (1.0/(2*Jmax))*Amax*Amax;
	V2 = Vf0m + (1.0/(2*Jmax))*Amax*Amax;
	Time->T7 = Time->T5 - TdiffAf;
	Time->T6 = (V1 - V2) / Amax;
      } else {
	// Amin isn't reached
	A1 = - sqrt(Jmax*(Vc-Vf0m));
	V1 = Vc - (1/(2*Jmax))*A1*A1;
	Time->T5 = - A1 / Jmax;
	Time->T7 = Time->T5 - TdiffAf;
	Time->T6 = 0.0;
      }
    }
  }

  return SM_OK;
}



SM_STATUS sm_CalculOfDistance( double V0, double A0, double Jmax, SM_TIMES_GLOBAL* Time, int* dir_a, int* dir_b, double* FinalDist)
{
  double Tjpa, Tjpa2, Tjpa3;
  double Taca, Taca2;
  double Tjna, Tjna2, Tjna3;
  double Tvc;
  double Tjnb, Tjnb2, Tjnb3;
  double Tacb, Tacb2;
  double Tjpb, Tjpb2, Tjpb3;
  int Dir_a, Dir_b;
  double A1, V1, X1, A2, V2, X2, A3, V3, X3, A4, V4, X4,A5, V5, X5, A6, V6, X6, Af;
  double FinalVel;
  double epsilon_times= 1e-08;
  double error = 0;

  Tjpa = Time->T1;
  Taca = Time->T2;
  Tjna = Time->T3;
  Tvc  = Time->T4;
  Tjnb = Time->T5;
  Tacb = Time->T6;
  Tjpb = Time->T7;
  Dir_a = *dir_a;
  Dir_b = *dir_b;

  Tjpa2 = Tjpa*Tjpa;
  Tjpa3 = Tjpa*Tjpa2;
  Taca2 = Taca*Taca;
  Tjna2 = Tjna*Tjna;
  Tjna3 = Tjna*Tjna2;
  Tjnb2 = Tjnb*Tjnb;
  Tjnb3 = Tjnb*Tjnb2;
  Tacb2 = Tacb*Tacb;
  Tjpb2 = Tjpb*Tjpb;
  Tjpb3 = Tjpb*Tjpb2;

  if (ABS(Tjpa) < epsilon_times) { Tjpa = 0;}
  if (ABS(Taca) < epsilon_times) { Taca = 0;}
  if (ABS(Tjna) < epsilon_times) { Tjna = 0;}
  if (ABS(Tvc) < epsilon_times)  { Tvc = 0;}
  if (ABS(Tjnb) < epsilon_times) { Tjnb = 0;}
  if (ABS(Tacb) < epsilon_times) { Tacb = 0;}
  if (ABS(Tjpb) < epsilon_times) { Tjpb = 0;}


  if (Tjpa < 0) { error =1; }
  if (Taca < 0) { error =1; }
  if (Tjna < 0) { error =1; }
  if (Tvc  < 0) { error =1; }
  if (Tjnb < 0) { error =1; }
  if (Tacb < 0) { error =1; }
  if (Tjpb < 0) { error =1; }


  // Acceleration, velocity and position evolution
  A1 = A0 + Dir_a*Jmax*Tjpa;
  V1 = V0 + A0*Tjpa + Dir_a*Jmax*Tjpa2/2.0;
  X1 = V0*Tjpa + A0*Tjpa2/2.0 + Dir_a*Jmax*Tjpa3/6.0;
  A2 = A1;
  V2 = V1 + A1*Taca;
  X2 = X1 + V1*Taca + A1*Taca2/2.0;
  A3 = A2 - Dir_a* Jmax*Tjna;
  V3 = V2 + A2*Tjna -  Dir_a*Jmax*Tjna2/2.0;
  X3 = X2 + V2*Tjna + A2*Tjna2/2.0 - Dir_a*Jmax*Tjna3/6.0;
  A4  = A3;
  V4  = V3;
  X4  = X3 + V3*Tvc;
  A5 = A4  +  Dir_b* Jmax*Tjnb;
  V5 = V4  +  A4*Tjnb +      Dir_b *Jmax*Tjnb2/2.0;
  X5 = X4  +  V4*Tjnb +  A4*Tjnb2/2.0 + Dir_b *Jmax*Tjnb3/6.0;
  A6 = A5;
  V6 = V5 + A5*Tacb;
  X6 = X5 + V5*Tacb + A5*Tacb2/2.0;

  Af = A6 - Dir_b*Jmax*Tjpb;
  FinalVel = V6 + A6*Tjpb    - Dir_b *Jmax*Tjpb2/2.0;
  *FinalDist = X6 + V6*Tjpb    + A6*Tjpb2/2.0    - Dir_b * Jmax*Tjpb3/6.0;

  if (error == 1) { return SM_ERROR; }

  return SM_OK;
}

SM_STATUS sm_AdjustTimeSlowingVelocityWithAcc(double V0, double Vf,double A0, double Af, double GoalDist, SM_LIMITS Limits, double TimeImp, SM_TIMES *Times, int* dir_a, int* dir_b)
{
  double FinalDist = 1000000000;
  double incr = 0.0;
  double a = 0;
  int nb_iteration = 0;
  double Vc = 0;
  //double Tsegments;
  double alpha;
  SM_COND IC, FC;
  SM_TIMES T_Jerk;
  int TrajectoryType;
  double VTvc;
  double A1, V1, A2, V2, A3;
  double Jmax;
  SM_TIMES_GLOBAL Time;

  Jmax = Limits.maxJerk;

  IC.a = A0;
  IC.v = V0;
  IC.x = 0.0;
  FC.a = Af;
  FC.v = Vf;
  FC.x = GoalDist;
  // 	Vc = 0;
  // 	sm_CalculTimeProfileWithVcFixedWithAcc( V0, Vf,A0, Af, Vc, Limits, &Time, dir_a, dir_b);
  // 	Tsegments = (Time.T1 + Time.T2 + Time.T3 + Time.T5 + Time.T6 + Time.T7);

  //  if (Tsegments <TimeImp) {
  //    printf("On a le temps de s'arreter \n");
  //    T.T4 = TimeImp - Tsegments;
  //    Times->Tjpa = T.T1;
  //    Times->Taca = T.T2;
  //    Times->Tjna = T.T3;
  //    Times->Tvc  = T.T4;
  //    Times->Tjnb = T.T5;
  //    Times->Tacb = T.T6;
  //    Times->Tjpb = T.T7;
  //    return SM_OK;
  //  }

  sm_ComputeSoftMotion(IC, FC, Limits, &T_Jerk, &TrajectoryType);

  if ( T_Jerk.Tjna > T_Jerk.Tjpa) {
    T_Jerk.Tjna = T_Jerk.Tjpa; /* if A0 = 0 */
  }

  A1 = Jmax * TrajectoryType * T_Jerk.Tjpa;
  V1 = V0 + Jmax * TrajectoryType * T_Jerk.Tjpa * T_Jerk.Tjpa / 2;
  A2 = A1;
  V2 = V1 + A1*T_Jerk.Taca;
  A3 = A2 - Jmax* TrajectoryType * T_Jerk.Tjna;
  VTvc = V2 + A2*T_Jerk.Tjna - Jmax* TrajectoryType * T_Jerk.Tjna * T_Jerk.Tjna /2;



  if ( VTvc > 0.0) {
    alpha = 1;
    //a = 0.1;
    a = Limits.maxVel;
    // 		 a = ABS(VTvc) + 0.01;
    // 		 if (a > Limits.maxVel) {
    // 			 a = Limits.maxVel;
    // 		 }
  } else {
    alpha = -1;
    //a = -0.1;
    a = -Limits.maxVel;
    // 		 a = - ABS(VTvc) - 0.01;
    // 		 if (a < -Limits.maxVel) {
    // 			 a = -Limits.maxVel;
    // 		 }
  }


  incr = ABS(VTvc / 100.0);

  do {
    Vc = a;
    sm_CalculTimeProfileWithVcFixedWithAcc( V0, Vf, A0, Af, Vc, Limits, &Time, dir_a, dir_b);
    Time.T4 = TimeImp - (Time.T1 + Time.T2 + Time.T3 + Time.T5 + Time.T6 + Time.T7);

    sm_CalculOfDistance( V0, A0, Limits.maxJerk, &Time, dir_a, dir_b, &FinalDist);

    nb_iteration = nb_iteration +1;

    if (alpha == 1) {
      if ( FinalDist > GoalDist) {
	a = a - incr;
      } else {
	a = a + incr;
	incr = incr/2;
      }
    } else {
      if ( FinalDist < GoalDist) {
	a = a + incr;
      } else {
	a = a - incr;
	incr = incr/2;
      }
    }

    //      if (VTvc > 0.0 && a < 0.0) {
    // 			 printf("xarm CANNOT ADJUST TIME SLOWING JERK 1 goalDist %f\n",GoalDist);
    //        return SM_ERROR;
    //      }
    //
    //      if (VTvc < 0.0 && a > 0.0) {
    // 			 printf("xarm CANNOT ADJUST TIME SLOWING JERK 2 goalDist %f\n",GoalDist);
    //        return SM_ERROR;
    //      }


    if (VTvc > 0.0 && a < 0.0) {
      //printf("xarm CANNOT ADJUST TIME SLOWING JERK 1 goalDist %f\n",GoalDist);
      //return SM_ERROR;
      a = a + incr;
      incr = incr/2;
    }

    if (VTvc < 0.0 && a > 0.0) {
      //printf("xarm CANNOT ADJUST TIME SLOWING JERK 2 goalDist %f\n",GoalDist);
      //return SM_ERROR;
      a = a - incr;
      incr = incr/2;
    }

    if (nb_iteration > 600) {
      printf("xarm CANNOT ADJUST TIME SLOWING JERK GD=%f V0=%f Vf%f a=%f TimeImp=%f VTvc %f\n",GoalDist, V0, Vf, a, TimeImp, VTvc);
      return SM_ERROR;
    }

  } while ( ABS(FinalDist - GoalDist) > 0.001);

  // printf("Ajustument en temps slowing velocity OK en %d iteration\n", nb_iteration);
  //	 printf("Vc %g FinalDist %g GoalDist %g\n",a,FinalDist,GoalDist);
  //	printf("Vc %g FinalDist %g GoalDist %g\n",a,FinalDist,GoalDist);
  //  xarmCalculTimeProfileWithVcFixed( V0, Vf, Vc, Limits, T, dir_a, dir_b);
  //  T.T4 = TimeImp - (T.T1 + T.T2 + T.T3 + T.T5 + T.T6 + T.T7);
  //  }
  //  sm_CalculTimeProfileWithVcFixed( V0, Vf, Vc, Limits, &T, dir_a, dir_b);
  if ( Time.T4 < 0.0 ) {
    printf("xarm CANNOT ADJUST TIME SLOWING JERK GD=%f V0=%f Vf%f a=%f TimeImp=%f\n",GoalDist, V0, Vf, a, TimeImp);
    printf("xarm Adjust Time Slowing Velocity Error Tvc < 0  goalDist %f\n",GoalDist);
    return SM_ERROR;
  }

  Times->Tjpa = Time.T1;
  Times->Taca = Time.T2;
  Times->Tjna = Time.T3;
  Times->Tvc  = Time.T4;
  Times->Tjnb = Time.T5;
  Times->Tacb = Time.T6;
  Times->Tjpb = Time.T7;

  return SM_OK;
}

SM_STATUS sm_AdjustTimeSlowingVelocity(double V0, double Vf, double GoalDist, SM_LIMITS Limits, double TimeImp, SM_TIMES *Times, int* dir_a, int* dir_b)
{
  double FinalDist = 1000000000;
  double incr = 0.0;
  double a = 0.0;
  int nb_iteration = 0;
  double Vc = 0.0;
  //double Tsegments;
  double alpha = 0.0;
  SM_COND IC, FC;
  SM_TIMES T_Jerk;
  int TrajectoryType = 0;
  double VTvc = 0.0;
  double A1=0, V1=0, A2=0, V2=0, A3=0;
  double Jmax=0.0;
  SM_TIMES_GLOBAL Time;
  Jmax = Limits.maxJerk;
  double A0 = 0.0;

  IC.a = 0.0;
  IC.v = V0;
  IC.x = 0.0;
  FC.a = 0.0;
  FC.v = Vf;
  FC.x = GoalDist;

  sm_ComputeSoftMotion(IC, FC, Limits, &T_Jerk, &TrajectoryType);

  if ( T_Jerk.Tjna > T_Jerk.Tjpa) {
    T_Jerk.Tjna = T_Jerk.Tjpa; /* if A0 = 0 */
  }

  A1 = Jmax * TrajectoryType * T_Jerk.Tjpa;
  V1 = V0 + Jmax * TrajectoryType * T_Jerk.Tjpa * T_Jerk.Tjpa / 2.0;
  A2 = A1;
  V2 = V1 + A1*T_Jerk.Taca;
  A3 = A2 - Jmax* TrajectoryType * T_Jerk.Tjna;
  VTvc = V2 + A2*T_Jerk.Tjna - Jmax* TrajectoryType * T_Jerk.Tjna * T_Jerk.Tjna /2.0;



  if ( VTvc > 0.0) {
    alpha = 1;
    //a = 0.1;
    //a = Limits.maxVel;
    a = ABS(VTvc) + 0.01;
    if (a > Limits.maxVel) {
      a = Limits.maxVel;
    }
  } else {
    alpha = -1;
    //a = -0.1;
    //  	a = -Limits.maxVel;
    a = - ABS(VTvc) - 0.01;
    if (a < -Limits.maxVel) {
      a = -Limits.maxVel;
    }
  }
  incr = ABS(VTvc / 100.0);

  do {
    Vc = a;
    sm_CalculTimeProfileWithVcFixed( V0, Vf, Vc, Limits, &Time, dir_a, dir_b);
    Time.T4 = TimeImp - (Time.T1 + Time.T2 + Time.T3 + Time.T5 + Time.T6 + Time.T7);

    sm_CalculOfDistance( V0, A0, Limits.maxJerk, &Time, dir_a, dir_b, &FinalDist);

    nb_iteration = nb_iteration +1;

    if (alpha == 1) {
      if ( FinalDist > GoalDist) {
	a = a - incr;
      } else {
	a = a + incr;
	incr = incr/2.0;
      }
    } else {
      if ( FinalDist < GoalDist) {
	a = a + incr;
      } else {
	a = a - incr;
	incr = incr/2.0;
      }
    }

    //      if (VTvc > 0.0 && a < 0.0) {
    // 			 printf("xarm CANNOT ADJUST TIME SLOWING JERK 1 goalDist %f\n",GoalDist);
    //        return SM_ERROR;
    //      }
    //
    //      if (VTvc < 0.0 && a > 0.0) {
    // 			 printf("xarm CANNOT ADJUST TIME SLOWING JERK 2 goalDist %f\n",GoalDist);
    //        return SM_ERROR;
    //      }


    if (VTvc > 0.0 && a < 0.0) {
      //printf("xarm CANNOT ADJUST TIME SLOWING JERK 1 goalDist %f\n",GoalDist);
      //return SM_ERROR;
      a = a + incr;
      incr = incr/2;
    }

    if (VTvc < 0.0 && a > 0.0) {
      //printf("xarm CANNOT ADJUST TIME SLOWING JERK 2 goalDist %f\n",GoalDist);
      //return SM_ERROR;
      a = a - incr;
      incr = incr/2;
    }

    if (nb_iteration > 1000) {

      sm_ComputeSoftMotion(IC, FC, Limits, &T_Jerk, &TrajectoryType);

      if ( T_Jerk.Tjna > T_Jerk.Tjpa) {
	T_Jerk.Tjna = T_Jerk.Tjpa; /* if A0 = 0 */
      }

      A1 = Jmax * TrajectoryType * T_Jerk.Tjpa;
      V1 = V0 + Jmax * TrajectoryType * T_Jerk.Tjpa * T_Jerk.Tjpa / 2.0;
      A2 = A1;
      V2 = V1 + A1*T_Jerk.Taca;
      A3 = A2 - Jmax* TrajectoryType * T_Jerk.Tjna;
      VTvc = V2 + A2*T_Jerk.Tjna - Jmax* TrajectoryType * T_Jerk.Tjna * T_Jerk.Tjna /2.0;




      // 			printf("xarm CANNOT ADJUST TIME SLOWING JERK GD=%f V0=%f Vf%f a=%f TimeImp=%f VTvc %f\n",GoalDist, V0, Vf, a, TimeImp, VTvc);


      return SM_ERROR;
    }

  } while ( ABS(FinalDist - GoalDist) > 0.0001);

  // printf("Ajustument en temps slowing velocity OK en %d iteration\n", nb_iteration);
  //	 printf("Vc %g FinalDist %g GoalDist %g\n",a,FinalDist,GoalDist);
  //	printf("Vc %g FinalDist %g GoalDist %g\n",a,FinalDist,GoalDist);
  //  xarmCalculTimeProfileWithVcFixed( V0, Vf, Vc, Limits, T, dir_a, dir_b);
  //  T.T4 = TimeImp - (T.T1 + T.T2 + T.T3 + T.T5 + T.T6 + T.T7);
  //  }
  //  sm_CalculTimeProfileWithVcFixed( V0, Vf, Vc, Limits, &T, dir_a, dir_b);
  if ( Time.T4 < 0.0 ) {
    // 		printf("CANNOT ADJUST TIME SLOWING Velocity GD=%f V0=%f Vf%f a=%f TimeImp=%f\n",GoalDist, V0, Vf, a, TimeImp);
    // 		printf("xarm Adjust Time Slowing Velocity Error Tvc < 0  goalDist %f\n",GoalDist);
    return SM_ERROR;
  }

  Times->Tjpa = Time.T1;
  Times->Taca = Time.T2;
  Times->Tjna = Time.T3;
  Times->Tvc  = Time.T4;
  Times->Tjnb = Time.T5;
  Times->Tacb = Time.T6;
  Times->Tjpb = Time.T7;

  return SM_OK;
}

SM_STATUS sm_AdjustTimeWithAcc(SM_COND IC, SM_COND FC,double MaxTime, SM_LIMITS Limits, SM_TIMES *Time, double* Jerk, int* DirTransition_a, int* DirTransition_b)
{
  SM_LIMITS newLimits;
  int DirTransition;
  // 	SM_COND IC, FC;
  SM_LIMITS LimitsVel;
  LimitsVel.maxJerk = Limits.maxJerk;
  LimitsVel.maxAcc = Limits.maxAcc;
  LimitsVel.maxVel = Limits.maxVel;

  // 	IC.a = TRUNC(IC_e.a);
  // 	IC.v = TRUNC(IC_e.v);
  // 	IC.x = TRUNC(IC_e.x);
  // 	FC.a = TRUNC(FC_e.a);
  // 	FC.v = TRUNC(FC_e.v);
  // 	FC.x = TRUNC(FC_e.x);
  //
  // 	printf("TRUNC(IC_e.a) %f   ,   %f\n",IC_e.a,TRUNC(IC_e.a));

  if(sm_AdjustTimeSlowingVelocityWithAcc(IC.v, FC.v,IC.a, FC.a, FC.x, LimitsVel, MaxTime, Time, DirTransition_a, DirTransition_b)!=0) {
    //printf("xarm Adjust Time Slowing Velocity ERROR\n");
    if(sm_AdjustTimeSlowingJerk(IC, FC, MaxTime, Limits, Time, Jerk, &newLimits, &DirTransition)!=0) {
      //printf("xarm Adjust Time Slowing Jerk ERROR\n");
      LimitsVel.maxJerk = Limits.maxJerk*3;
      LimitsVel.maxAcc = Limits.maxAcc;
      LimitsVel.maxVel = Limits.maxVel;
      if(sm_AdjustTimeSlowingVelocityWithAcc(IC.v, FC.v,IC.a, FC.a, FC.x, LimitsVel, MaxTime, Time, DirTransition_a, DirTransition_b)!=0) {
	printf("xarm Adjust Time Slowing Velocity ERROR with higher Jerk\n");
	return SM_ERROR;
      } else {
	printf("CAUTION Adjust Time solved with higher Jerk\n");
	*Jerk = LimitsVel.maxJerk;
	return SM_OK;
      }
    } else {
      //printf("xarm Adjust Time Slowing Jerk OK\n");
      * Jerk = newLimits.maxJerk;
      * DirTransition_a = DirTransition;
      * DirTransition_b = -DirTransition;
      return SM_OK;
    }
  } else {
    //printf("xarm Adjust Time Slowing Velocity OK\n");
    *Jerk = LimitsVel.maxJerk;
    return SM_OK;
  }
}

SM_STATUS sm_AdjustTime(SM_COND IC, SM_COND FC,double MaxTime, SM_LIMITS Limits, SM_TIMES *Time, double* Jerk, int* DirTransition_a, int* DirTransition_b)
{
  SM_LIMITS newLimits;
  int DirTransition;
  // 	SM_COND IC, FC;
  SM_LIMITS LimitsVel;
  LimitsVel.maxJerk = Limits.maxJerk;
  LimitsVel.maxAcc = Limits.maxAcc;
  LimitsVel.maxVel = Limits.maxVel;

  // 	IC.a = TRUNC(IC_e.a);
  // 	IC.v = TRUNC(IC_e.v);
  // 	IC.x = TRUNC(IC_e.x);
  // 	FC.a = TRUNC(FC_e.a);
  // 	FC.v = TRUNC(FC_e.v);
  // 	FC.x = TRUNC(FC_e.x);
  //
  // 	printf("TRUNC(IC_e.a) %f   ,   %f\n",IC_e.a,TRUNC(IC_e.a));

  if(sm_AdjustTimeSlowingVelocity(IC.v, FC.v, FC.x, LimitsVel, MaxTime, Time, DirTransition_a, DirTransition_b)!=0) {
    //printf("xarm Adjust Time Slowing Velocity ERROR\n");
    if(sm_AdjustTimeSlowingJerk(IC, FC, MaxTime, Limits, Time, Jerk, &newLimits, &DirTransition)!=0) {
      //printf("xarm Adjust Time Slowing Jerk ERROR\n");
      LimitsVel.maxJerk = Limits.maxJerk*3;
      LimitsVel.maxAcc = Limits.maxAcc;
      LimitsVel.maxVel = Limits.maxVel;
      if(sm_AdjustTimeSlowingVelocity(IC.v, FC.v, FC.x, LimitsVel, MaxTime, Time, DirTransition_a, DirTransition_b)!=0) {
	printf("xarm Adjust Time Slowing Velocity ERROR with higher Jerk\n");
	return SM_ERROR;
      } else {
	printf("CAUTION Adjust Time solved with higher Jerk\n");
	*Jerk = LimitsVel.maxJerk;
	return SM_OK;
      }
    } else {
      //printf("xarm Adjust Time Slowing Jerk OK\n");
      * Jerk = newLimits.maxJerk;
      * DirTransition_a = DirTransition;
      * DirTransition_b = -DirTransition;
      return SM_OK;
    }
  } else {
    //printf("xarm Adjust Time Slowing Velocity OK\n");
    *Jerk = LimitsVel.maxJerk;
    return SM_OK;
  }
}


SM_STATUS sm_VerifyTimes_Dir_ab(double dist_error, double GD, SM_JERKS Jerks, SM_COND IC, int Dir_a, int Dir_b, SM_TIMES Time, SM_COND *FC, SM_TIMES *Acc, SM_TIMES *Vel, SM_TIMES *Pos)
{
  double J1, J2, J3, J4;
  double Tjpa  = Time.Tjpa;
  double Tjpa2 = Tjpa*Tjpa;
  double Tjpa3 = Tjpa*Tjpa2;
  double Taca  = Time.Taca;
  double Taca2 = Taca*Taca;
  double Tjna  = Time.Tjna;
  double Tjna2 = Tjna*Tjna;
  double Tjna3 = Tjna*Tjna2;
  double Tvc   = Time.Tvc;
  double Tjnb  = Time.Tjnb;
  double Tjnb2 = Tjnb*Tjnb;
  double Tjnb3 = Tjnb*Tjnb2;
  double Tacb  = Time.Tacb;
  double Tacb2 = Tacb*Tacb;
  double Tjpb  = Time.Tjpb;
  double Tjpb2 = Tjpb*Tjpb;
  double Tjpb3 = Tjpb*Tjpb2;
  double ErrorDistance = 0.0;

  /* Final Conditions Initialitation */
  FC->a = -1000;
  FC->v = -1000;
  FC->x = -1000;

  if (Tjpa<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tjpa negative %f\n", Tjpa);
    return SM_ERROR;
  }
  if (Taca<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Taca negative %f\n",Taca);
    return SM_ERROR;
  }
  if (Tjna<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tjna negative %f\n",Tjna);
    return SM_ERROR;
  }
  if (Tvc<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tvc negative %f\n",Tvc);
    return SM_ERROR;
  }
  if (Tjnb<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tjnb negative %f\n",Tjnb);
    return SM_ERROR;
  }
  if (Tacb<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tacb negative %f\n",Tacb);
    return SM_ERROR;
  }
  if (Tjpb<0) {
    fprintf(stderr,"verify Times - SM_ERROR - Tjpb negative %f\n",Tjpb);
    return SM_ERROR;
  }

  /* Jerks Option */
  if (Jerks.sel ==1) {
    J1 = Dir_a *Jerks.J1;
    J2 = Dir_a *Jerks.J1;
    J3 = Dir_b *Jerks.J1;
    J4 = Dir_b *Jerks.J1;
  }
  else {
    J1 = Dir_a *Jerks.J1;
    J2 = Dir_a *Jerks.J2;
    J3 = Dir_b *Jerks.J3;
    J4 = Dir_b *Jerks.J4;
  }

  /* Acceleration, velocity and position evolution */
  Acc->Tjpa = IC.a      +      J1*Tjpa;
  Vel->Tjpa = IC.v      +      IC.a*Tjpa +        J1*Tjpa2/2;
  Pos->Tjpa = IC.x      +      IC.v*Tjpa +      IC.a*Tjpa2/2 + J1*Tjpa3/6;

  Acc->Taca = Acc->Tjpa;
  Vel->Taca = Vel->Tjpa + Acc->Tjpa*Taca;
  Pos->Taca = Pos->Tjpa + Vel->Tjpa*Taca + Acc->Tjpa*Taca2/2;

  Acc->Tjna = Acc->Taca -        J2*Tjna;
  Vel->Tjna = Vel->Taca + Acc->Taca*Tjna -        J2*Tjna2/2;
  Pos->Tjna = Pos->Taca + Vel->Taca*Tjna + Acc->Taca*Tjna2/2 - J2*Tjna3/6;

  Acc->Tvc  = Acc->Tjna;
  Vel->Tvc  = Vel->Tjna;
  Pos->Tvc  = Pos->Tjna + Vel->Tjna*Tvc;

  Acc->Tjnb = Acc->Tvc  +        J3*Tjnb;
  Vel->Tjnb = Vel->Tvc  +  Acc->Tvc*Tjnb +        J3*Tjnb2/2;
  Pos->Tjnb = Pos->Tvc  +  Vel->Tvc*Tjnb +  Acc->Tvc*Tjnb2/2 + J3*Tjnb3/6;

  Acc->Tacb = Acc->Tjnb;
  Vel->Tacb = Vel->Tjnb + Acc->Tjnb*Tacb;
  Pos->Tacb = Pos->Tjnb + Vel->Tjnb*Tacb + Acc->Tjnb*Tacb2/2;

  Acc->Tjpb = Acc->Tacb -        J4*Tjpb;
  Vel->Tjpb = Vel->Tacb + Acc->Tacb*Tjpb -        J4*Tjpb2/2;
  Pos->Tjpb = Pos->Tacb + Vel->Tacb*Tjpb + Acc->Tacb*Tjpb2/2 - J4*Tjpb3/6;

  /* Final Conditions */
  FC->a = Acc->Tjpb;
  FC->v = Vel->Tjpb;
  FC->x = Pos->Tjpb;

  /* Error Distance */
  ErrorDistance = GD + IC.x - Pos->Tjpb;

  if (fabs(ErrorDistance)> dist_error ) {

    fprintf(stderr,"Verify Times - SM_ERROR - Error Distance \n");
    fprintf(stderr,"ED = %f \n",ErrorDistance);
    printf("GD = %f \n",GD);
    //    printf("IC->a = %f\n",IC.a);
    //    printf("IC->v = %f\n",IC.v);
    //    printf("IC->x = %f\n",IC.x);
    //    printf("FC->a = %f\n",FC->a);
    //    printf("FC->v = %f\n",FC->v);
    //    printf("FC->x = %f\n",FC->x);
    //    printf("Dir_a = %d\n",Dir_a);
    //    printf("Dir_b = %d\n",Dir_b);
    //    printf("Jerk = %f\n",Jerks.J1);
    //    printf("Pos->Tjpa= %f\n",Pos->Tjpa);
    //    printf("Pos->Taca= %f\n",Pos->Taca);
    //    printf("Pos->Tjna= %f\n",Pos->Tjna);
    //    printf("Pos->Tvc= %f\n",Pos->Tvc);
    //    printf("Pos->Tjnb= %f\n",Pos->Tjnb);
    //    printf("Pos->Tacb= %f\n",Pos->Tacb);
    //    printf("Pos->Tjpb= %f\n",Pos->Tjpb);
    //    printf("Tjpa= %f\n",Tjpa);
    //    printf("Taca= %f\n",Taca);
    //    printf("Tjna= %f\n",Tjna);
    //    printf("Tvc= %f\n", Tvc);
    //    printf("Tjnb= %f\n",Tjnb);
    //    printf("Tacb= %f\n",Tacb);
    //    printf("Tjpb= %f\n",Tjpb);
    return SM_ERROR;
  }
  return SM_OK;
}

SM_STATUS sm_FindTransitionTime( SM_POSELIMITS limitsGoto, SM_TRANSITION_MOTION* motion, int *impTimeM)
{
  int k = 0;
  int i=0, j=0;
  SM_COND ICl, FCl;
  SM_TIMES T_Jerk;
  SM_LIMITS auxLimits;
  double imposedTime;
  int dir_a, dir_b;
  double V0, Vf;
  int interval[SM_NB_DIM][300];
  int imposedTimeM;

  /* Init */
  for (i=0; i<SM_NB_DIM; i++) {
    for (k=0; k<300;k++) {
      interval[i][k] = 0;
    }
  }
  k=0;

  for(i=0; i<SM_NB_DIM; i++) {

    if (i < 3) {
      // cartesien
      /*Set Limits*/
      auxLimits.maxVel  = limitsGoto.linear.maxVel;
      auxLimits.maxAcc  = limitsGoto.linear.maxAcc;
      auxLimits.maxJerk = limitsGoto.linear.maxJerk * 3;
    } else {
      //rotation
      auxLimits.maxVel  = limitsGoto.linear.maxVel;
      auxLimits.maxAcc  = limitsGoto.linear.maxAcc;
      auxLimits.maxJerk = limitsGoto.linear.maxJerk * 3;
    }

    ICl.a = motion->IC[i].a;
    ICl.v = motion->IC[i].v;
    ICl.x = 0;
    FCl.a = motion->FC[i].a;
    FCl.v = motion->FC[i].v;
    FCl.x = (motion->FC[i].x - motion->IC[i].x) ;

    printf("IC %d :   %f %f %f %f %f %f\n",i,ICl.a,ICl.v,ICl.x ,FCl.a,FCl.v,FCl.x );

    /* set initial and final conditions */
    V0 = ICl.v;
    Vf = FCl.v;

    //  printf("axis %d stopTimeM %d optimalTimeM[i] %d \n",i,stopTimeM,optimalTimeM[i]);

    for(j = motion->timeToStop; j<= motion->timeToStop+5 ; j++) {
      interval[i][j] = 1;
    }

    // interval[i][motion->timeToStop] = 1;

    for(imposedTimeM = motion->optimalTime[i]; imposedTimeM < motion->timeToStop ;  imposedTimeM = imposedTimeM + 1) {

      imposedTime = imposedTimeM /100.0;
      if(sm_AdjustTimeSlowingVelocity(V0, Vf, FCl.x, auxLimits, imposedTime, &T_Jerk, &dir_a, &dir_b)!=0) {
	interval[i][imposedTimeM] = 0;
      } else {
	interval[i][imposedTimeM] = 1;
      }
    }

  } /* end for i */

  /* Find time to impose */
  k=0;
  for(k=0 ; k<= motion->timeToStop + 2; k++) {
    if (interval[0][k]==1 && interval[1][k]==1 && interval[2][k]==1 && interval[3][k]==1 && interval[4][k]==1 && interval[5][k]==1 ){
      //   if (interval[0][k]==1 && interval[1][k]==1 && interval[2][k]==1 && interval[3][k]==1 && interval[4][k]==1 && interval[5][k]==1 && interval[6][k]==1){
      break;
    }
  }

  *impTimeM =  k;

  imposedTime = k /100.0;
  // printf("imposedTime %f  impTimeM %d \n",imposedTime,*impTimeM);

  if (*impTimeM >=  motion->timeToStop) {
    printf("*impTimeM >= stopTimeM %d\n",*impTimeM);
    return SM_OK;
  }

  for(i=0; i<SM_NB_DIM; i++) {

    if (i < 3) {
      /*Set Limits*/
      auxLimits.maxVel  = limitsGoto.linear.maxVel;
      auxLimits.maxAcc  = limitsGoto.linear.maxAcc;
      auxLimits.maxJerk = limitsGoto.linear.maxJerk * 3;
    } else {
      auxLimits.maxVel  = limitsGoto.linear.maxVel;
      auxLimits.maxAcc  = limitsGoto.linear.maxAcc;
      auxLimits.maxJerk = limitsGoto.linear.maxJerk * 3;
    }

    motion->jerk[i].J1 = auxLimits.maxJerk;

    ICl.a = motion->IC[i].a;
    ICl.v = motion->IC[i].v;
    ICl.x = 0;
    FCl.a = motion->FC[i].a;
    FCl.v = motion->FC[i].v;
    FCl.x = (motion->FC[i].x - motion->IC[i].x) ;

    /* set initial and final conditions */
    V0 = ICl.v;
    Vf = FCl.v;

    if(sm_AdjustTimeSlowingVelocity(V0, Vf, FCl.x, auxLimits, imposedTime, &(motion->Times[i]), &(motion->Dir_a[i]), &(motion->Dir_b[i]))!=0) {
      return SM_ERROR;
    }
  }

  return SM_OK;
}

/* Verify Times values and goal distance
   tpVerifyTimes(DISTANCE_TOLERANCE, double GD, SM_JERKS Jerks, SM_COND IC, int Dir, SM_TIMES T, SM_COND *FC, SM_TIMES *Acc, SM_TIMES *Vel, SM_TIMES *Pos, SM_SELECT msgError )

   Inputs:   GD         Goal Distance
   Jerks      Jerks
   IC         Initial Conditions  A0, V0, X0
   Dir        Direction
   T          Times  = Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb
   msgError   Display Error Messages SM_ON / SM_OFF

   Outputs:  FC         Final Conditions    AF, VF, XF
   Acc        Acceleration values
   Vel        Velocity values
   Pos        Position values

   Return:   SM_OK
   SM_ERROR      - Negative time
   - Fail Distance
*/
SM_STATUS sm_VerifyTimes(double dist_error, double GD, SM_JERKS Jerks, SM_COND IC, int Dir, SM_TIMES Time, SM_COND *FC, SM_TIMES *Acc, SM_TIMES *Vel, SM_TIMES *Pos, SM_SELECT msgError)
{
  double J1, J2, J3, J4;
  double Tjpa  = Time.Tjpa;
  double Tjpa2 = Tjpa*Tjpa;
  double Tjpa3 = Tjpa*Tjpa2;
  double Taca  = Time.Taca;
  double Taca2 = Taca*Taca;
  double Tjna  = Time.Tjna;
  double Tjna2 = Tjna*Tjna;
  double Tjna3 = Tjna*Tjna2;
  double Tvc   = Time.Tvc;
  double Tjnb  = Time.Tjnb;
  double Tjnb2 = Tjnb*Tjnb;
  double Tjnb3 = Tjnb*Tjnb2;
  double Tacb  = Time.Tacb;
  double Tacb2 = Tacb*Tacb;
  double Tjpb  = Time.Tjpb;
  double Tjpb2 = Tjpb*Tjpb;
  double Tjpb3 = Tjpb*Tjpb2;
  double ErrorDistance = 0.0;

  /* Final Conditions Initialitation */
  FC->a = -1000;
  FC->v = -1000;
  FC->x = -1000;

  if (Tjpa<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tjpa negative %f\n", Tjpa);
    return SM_ERROR;
  }
  if (Taca<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Taca negative %f\n",Taca);
    return SM_ERROR;
  }

  if (Tjna<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tjna negative %f\n",Tjna);
    return SM_ERROR;
  }

  if (Tvc<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tvc negative %f\n",Tvc);
    return SM_ERROR;
  }

  if (Tjnb<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tjnb negative %f\n",Tjnb);
    return SM_ERROR;
  }

  if (Tacb<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tacb negative %f\n",Tacb);
    return SM_ERROR;
  }

  if (Tjpb<0) {
    if (!msgError)
      fprintf(stderr,"verify Times - SM_ERROR - Tjpb negative %f\n",Tjpb);
    return SM_ERROR;
  }

  /* Jerks Option */
  if (Jerks.sel ==1) {
    J1 = Dir*Jerks.J1;
    J2 = Dir*Jerks.J1;
    J3 = Dir*Jerks.J1;
    J4 = Dir*Jerks.J1;
  }
  else {
    J1 = Dir*Jerks.J1;
    J2 = Dir*Jerks.J2;
    J3 = Dir*Jerks.J3;
    J4 = Dir*Jerks.J4;
  }

  /* Acceleration, velocity and position evolution */
  Acc->Tjpa = IC.a      +        J1*Tjpa;
  Vel->Tjpa = IC.v      +      IC.a*Tjpa +        J1*Tjpa2/2;
  Pos->Tjpa = IC.x      +      IC.v*Tjpa +      IC.a*Tjpa2/2 + J1*Tjpa3/6;

  Acc->Taca = Acc->Tjpa;
  Vel->Taca = Vel->Tjpa + Acc->Tjpa*Taca;
  Pos->Taca = Pos->Tjpa + Vel->Tjpa*Taca + Acc->Tjpa*Taca2/2;

  Acc->Tjna = Acc->Taca -        J2*Tjna;
  Vel->Tjna = Vel->Taca + Acc->Taca*Tjna -        J2*Tjna2/2;
  Pos->Tjna = Pos->Taca + Vel->Taca*Tjna + Acc->Taca*Tjna2/2 - J2*Tjna3/6;

  Acc->Tvc  = Acc->Tjna;
  Vel->Tvc  = Vel->Tjna;
  Pos->Tvc  = Pos->Tjna + Vel->Tjna*Tvc;

  Acc->Tjnb = Acc->Tvc  -        J3*Tjnb;
  Vel->Tjnb = Vel->Tvc  +  Acc->Tvc*Tjnb -        J3*Tjnb2/2;
  Pos->Tjnb = Pos->Tvc  +  Vel->Tvc*Tjnb +  Acc->Tvc*Tjnb2/2 - J3*Tjnb3/6;

  Acc->Tacb = Acc->Tjnb;
  Vel->Tacb = Vel->Tjnb + Acc->Tjnb*Tacb;
  Pos->Tacb = Pos->Tjnb + Vel->Tjnb*Tacb + Acc->Tjnb*Tacb2/2;

  Acc->Tjpb = Acc->Tacb +        J4*Tjpb;
  Vel->Tjpb = Vel->Tacb + Acc->Tacb*Tjpb +        J4*Tjpb2/2;
  Pos->Tjpb = Pos->Tacb + Vel->Tacb*Tjpb + Acc->Tacb*Tjpb2/2 + J4*Tjpb3/6;

  /* Final Conditions */
  FC->a = Acc->Tjpb;
  FC->v = Vel->Tjpb;
  FC->x = Pos->Tjpb;

  /* Error Distance */
  ErrorDistance = Dir*GD + IC.x - Pos->Tjpb;

  if (fabs(ErrorDistance)> dist_error ) {
    if (!msgError) {
      fprintf(stderr,"Verify Times - SM_ERROR - Error Distance \n");
      fprintf(stderr,"ED = %f \n",ErrorDistance);
      printf("GD = %f \n",GD);
      printf("IC->a = %f\n",IC.a);
      printf("IC->v = %f\n",IC.v);
      printf("IC->x = %f\n",IC.x);
      printf("FC->a = %f\n",FC->a);
      printf("FC->v = %f\n",FC->v);
      printf("FC->x = %f\n",FC->x);
      printf("Dir = %d\n",Dir);
      printf("Jerk = %f\n",Jerks.J1);
      printf("Pos->Tjpa= %f\n",Pos->Tjpa);
      printf("Pos->Taca= %f\n",Pos->Taca);
      printf("Pos->Tjna= %f\n",Pos->Tjna);
      printf("Pos->Tvc= %f\n",Pos->Tvc);
      printf("Pos->Tjnb= %f\n",Pos->Tjnb);
      printf("Pos->Tacb= %f\n",Pos->Tacb);
      printf("Pos->Tjpb= %f\n",Pos->Tjpb);
      printf("Tjpa= %f\n",Tjpa);
      printf("Taca= %f\n",Taca);
      printf("Tjna= %f\n",Tjna);
      printf("Tvc= %f\n", Tvc);
      printf("Tjnb= %f\n",Tjnb);
      printf("Tacb= %f\n",Tacb);
      printf("Tjpb= %f\n",Tjpb);
    }
    return SM_ERROR;
  }
  return SM_OK;
}

SM_STATUS sm_CalculOfAccVelPosAtTime(int timeM, SM_SEGMENT* seg, SM_COND* v)
{
  double k1, k2, k3;
  double Ts2, Ts3;
  int i;

  Ts2 = (double)(SM_S_T*SM_S_T)/(SM_S_TD*SM_S_TD);
  Ts3 = (double)(SM_S_T*Ts2)/SM_S_TD;
  i = timeM;

  //   printf(" TYPE of Segment %d\n",seg->type);
  switch(seg->type) {
  case 1:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = (double) i*i*Ts2/2.0;
    k3 = (double) i*i*i*Ts3/6.0;
    v->a = (seg->A0) + (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 + (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 + (seg->dir)*(seg->J)*k3;
    // 			printf("k1 %f k2 %f k3 %f seg->J %f\n",k1,k2,k3,seg->J);
    // 			printf("seg->A0 %f seg->V0 %f seg->X0 %f\n",seg->A0, seg->V0, seg->X0 );
    // 			printf("v->a %f v->v %f v->x %f\n",v->a, v->v, v->x );
    break;
  case 2:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = i*i*Ts2/2;
    v->a = (seg->A0);
    v->v = (seg->V0) + (seg->A0)*k1;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2;
    break;
  case 3:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = i*i*Ts2/2;
    k3 = i*i*i*Ts3/6;
    v->a = (seg->A0) - (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0)+  (seg->A0)*k1 - (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 - (seg->dir)*(seg->J)*k3;
    break;
  case 4:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    v->a = (seg->A0);
    v->v = (seg->V0);
    v->x = (seg->X0) + (seg->V0)*k1;
    break;
  case 5:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = i*i*Ts2/2;
    k3 = i*i*i*Ts3/6;
    v->a = (seg->A0) - (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 - (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 - (seg->dir)*(seg->J)*k3;
    break;
  case 6:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = i*i*Ts2/2;
    v->a = (seg->A0);
    v->v = (seg->V0) + (seg->A0)*k1;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2;
    break;
  case 7:
    k1 = (double)(SM_S_T*i)/SM_S_TD;
    k2 = i*i*Ts2/2;
    k3 = i*i*i*Ts3/6;
    v->a = (seg->A0) + (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 + (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 + (seg->dir)*(seg->J)*k3;
    break;
  default:
    printf("xarm Bad type of Segment\n");
    return SM_ERROR;
  }

  if (isnan(v->x) || isnan(v->v)  || isnan(v->a)) {
    printf("PROBLEME\n");
    printf("seg->X0 %g seg->V0 %g seg->A0 %g seg->J %g seg->dir %d\n",seg->X0 , seg->V0 ,seg->A0 ,seg->J ,seg->dir);
    return SM_ERROR;
  }

  return SM_OK;
}



SM_STATUS sm_CalculOfAccVelPosAtTimeSecond(double t, SM_SEGMENT* seg, SM_COND* v)
{
  double k1, k2, k3;
  k1 = t;
  k2 = t*t/2.0;
  k3 = t*t*t/6.0;

  //   printf(" TYPE of Segment %d\n",seg->type);
  switch(seg->type) {
  case 1:
    v->a = (seg->A0) + (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 + (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 + (seg->dir)*(seg->J)*k3;
    break;
  case 2:
    v->a = (seg->A0);
    v->v = (seg->V0) + (seg->A0)*k1;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2;
    break;
  case 3:
    v->a = (seg->A0) - (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0)+  (seg->A0)*k1 - (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 - (seg->dir)*(seg->J)*k3;
    break;
  case 4:
    v->a = (seg->A0);
    v->v = (seg->V0);
    v->x = (seg->X0) + (seg->V0)*k1;
    break;
  case 5:
    v->a = (seg->A0) - (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 - (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 - (seg->dir)*(seg->J)*k3;
    break;
  case 6:
    v->a = (seg->A0);
    v->v = (seg->V0) + (seg->A0)*k1;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2;
    break;
  case 7:
    v->a = (seg->A0) + (seg->dir)*(seg->J)*k1;
    v->v = (seg->V0) + (seg->A0)*k1 + (seg->dir)*(seg->J)*k2;
    v->x = (seg->X0) + (seg->V0)*k1 + (seg->A0)*k2 + (seg->dir)*(seg->J)*k3;
    break;
  default:
    printf("xarm Bad type of Segment\n");
    return SM_ERROR;
  }

  if (isnan(v->x) || isnan(v->v)  || isnan(v->a)) {
    printf("PROBLEME\n");
    printf("seg->X0 %g seg->V0 %g seg->A0 %g seg->J %g seg->dir %d t %f\n",seg->X0 , seg->V0 ,seg->A0 ,seg->J ,seg->dir, t);
    return SM_ERROR;
  }

  return SM_OK;
}

/* Adjust Time to Sampling_Time Multiple
   sm_SamplingAdjustTime(double Time, double aTime)

   input:   Time      Time Valor

   output:  aTime     Adjusted time

   return:  SM_OK
   SM_ERROR
*/
SM_STATUS sm_SamplingAdjustTime(double Time, double *aTime)
{
  *aTime = (int)(Time*SM_NB_TICK_SEC) * SM_SAMPLING_TIME;
  return SM_OK;
}

/* Get Monotonic Times and Number of Elements
   sm_GetMonotonicTimes(TP_TIMES Times, TP_TIMES *TM, int *NOE)
   Inputs:   Times          Times  = Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb

   Outputs:  NOE        Number Of Elements
   TM         Times Monotonic = Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb

   Return:   SM_OK
   SM_ERROR      - Sampling Adjust Time
   - Memory Failled
   - Fail Distance
*/
SM_STATUS sm_GetMonotonicTimes(SM_TIMES Times,SM_TIMES *TM, int *NOE)
{
  double Tjpa, Taca, Tjna, Tvc, Tjnb, Tacb, Tjpb;
  int auxNOE =0;

  *NOE = 0;

  /* Adjusting for Sampling Time */
  if (sm_SamplingAdjustTime(Times.Tjpa, &Tjpa)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tjpa \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Taca, &Taca)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Taca \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Tjna, &Tjna)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tjna \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Tvc, &Tvc)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tvc \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Tjnb, &Tjnb)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tjnb \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Tacb, &Tacb)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tacb \n");
    return SM_ERROR;
  }
  if (sm_SamplingAdjustTime(Times.Tjpb, &Tjpb)!= SM_OK) {
    fprintf(stderr, "sm Get Monotonic Times S D - SM_ERROR - sm Sampling Adjust Time Tjpb \n");
    return SM_ERROR;
  }
  /* TJPA INTERVAL */
  if (Tjpa) {
    auxNOE = (int)(Tjpa*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TACA INTERVAL */
  if (Taca) {
    auxNOE = (Taca*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TJNA INTERVAL */
  if (Tjna) {
    auxNOE = (Tjna*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TVC INTERVAL */
  if (Tvc) {
    auxNOE = (Tvc*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TJNB INTERVAL */
  if (Tjnb) {
    auxNOE = (Tjnb*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TACB INTERVAL */
  if (Tacb) {
    auxNOE = (Tacb*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE) - 1;
    else
      *NOE += ceil(auxNOE);
  }
  /* TJPB INTERVAL */
  if (Tjpb) {
    auxNOE = (Tjpb*SM_S_TD)/SM_S_T;
    if (ceil(auxNOE) - auxNOE > 0.0001)
      *NOE += ceil(auxNOE);
    else
      *NOE += ceil(auxNOE)+1;
  }

  /* *NOE += 2; */

  TM->Tjpa = Tjpa;
  TM->Taca = Taca;
  TM->Tjna = Tjna;
  TM->Tvc  = Tvc;
  TM->Tjnb = Tjnb;
  TM->Tacb = Tacb;
  TM->Tjpb = Tjpb;

  return SM_OK;
}


SM_STATUS sm_GetNumberOfElement (SM_TIMES* TM, SM_TIMES* TNE)
{
  TNE->Tjpa = TM->Tjpa*(SM_S_TD/SM_S_T);
  TNE->Taca = TM->Taca*(SM_S_TD/SM_S_T);
  TNE->Tjna = TM->Tjna*(SM_S_TD/SM_S_T);
  TNE->Tvc  = TM->Tvc*(SM_S_TD/SM_S_T);
  TNE->Tjnb = TM->Tjnb*(SM_S_TD/SM_S_T);
  TNE->Tacb = TM->Tacb*(SM_S_TD/SM_S_T);
  TNE->Tjpb = TM->Tjpb*(SM_S_TD/SM_S_T);
  return SM_OK;
}

void sm_SM_TIMES_copy_into(const SM_TIMES *e, SM_TIMES* s)
{
  s->Tjpa = e->Tjpa;
  s->Taca = e->Taca;
  s->Tjna = e->Tjna;
  s->Tvc  = e->Tvc ;
  s->Tjnb = e->Tjnb;
  s->Tacb = e->Tacb;
  s->Tjpb = e->Tjpb;
  return;
}

void sm_sum_motionTimes(SM_TIMES* sm_times, double* sum)
{
  *sum = sm_times->Tjpa \
    + sm_times->Taca \
    + sm_times->Tjna \
    + sm_times->Tvc  \
    + sm_times->Tjnb \
    + sm_times->Tacb \
    + sm_times->Tjpb;
  return;
}

// void sm_alloc_SM_MOTION_DYN(SM_MOTION_DYN* motion, int dim) {
// 	int i=0;
//
// 	for (i=0; i<SM_NB_DIM;i++) {
// 		motion->Times[i].seg = MY_ALLOC(double, dim);
// 		motion->Times[i].nseg = dim;
// 		motion->TimesM[i].seg = MY_ALLOC(double, dim);
// 		motion->TimesM[i].nseg = dim;
// 		motion->Acc[i].seg = MY_ALLOC(double, dim);
// 		motion->Acc[i].nseg = dim;
// 		motion->Vel[i].seg = MY_ALLOC(double, dim);
// 		motion->Vel[i].nseg = dim;
// 		motion->Pos[i].seg = MY_ALLOC(double, dim);
// 		motion->Pos[i].nseg = dim;
// 		motion->Jerk[i] = MY_ALLOC(double, dim);
// 		motion->Dir[i] = MY_ALLOC(int, dim);
// 		motion->TimeCumulM[i] = MY_ALLOC(int,dim);
// 	}
// 	return;
// }
//
// void sm_destroy_SM_MOTION_DYN(SM_MOTION_DYN* motion) {
// 	int i=0;
// 	int dim=0;
//
// 	dim = motion->Times[i].nseg;
//
// 	for (i=SM_NB_DIM-1; i>0;i--) {
// 		MY_FREE(motion->Times[i].seg, double, dim);
// 		MY_FREE(motion->TimesM[i].seg, double, dim);
// 		MY_FREE(motion->Acc[i].seg, double, dim);
// 		MY_FREE(motion->Vel[i].seg, double, dim);
// 		MY_FREE(motion->Pos[i].seg, double, dim);
// 		MY_FREE(motion->Jerk[i], double, dim);
// 		MY_FREE(motion->Dir[i], int, dim);
// 		MY_FREE(motion->TimeCumulM[i], int,dim);
// 	}
// 	return;
// }




void sm_copy_SM_MOTION_into(const SM_MOTION* e, SM_MOTION* s)
{
  int i=0, j=0;
  sm_SM_TIMES_copy_into(&e->TNE, &s->TNE);
  for (i=0; i<SM_NB_DIM; i++) {
    sm_SM_TIMES_copy_into(&(e->Times[i]), &(s->Times[i]));
    sm_SM_TIMES_copy_into(&(e->TimesM[i]), &(s->TimesM[i]));
    sm_SM_TIMES_copy_into(&(e->Acc[i]), &(s->Acc[i]));
    sm_SM_TIMES_copy_into(&(e->Vel[i]), &(s->Vel[i]));
    sm_SM_TIMES_copy_into(&(e->Pos[i]), &(s->Pos[i]));
    s->jerk[i].sel = e->jerk[i].sel;
    s->jerk[i].J1 = e->jerk[i].J1;
    s->jerk[i].J2 = e->jerk[i].J2;
    s->jerk[i].J3 = e->jerk[i].J3;
    s->jerk[i].J4 = e->jerk[i].J4;
    s->IC[i].a = e->IC[i].a;
    s->IC[i].v = e->IC[i].v;
    s->IC[i].x = e->IC[i].x;
    s->FC[i].a = e->FC[i].a;
    s->FC[i].v = e->FC[i].v;
    s->FC[i].x = e->FC[i].x;
    s->Dir[i] = e->Dir[i];
    s->Dir_a[i] = e->Dir_a[i];
    s->Dir_b[i] = e->Dir_b[i];
    s->MotionDuration[i] = e->MotionDuration[i];
    s->MotionDurationM[i] = e->MotionDurationM[i];
    s->motionIsAdjusted[i] = e->motionIsAdjusted[i];
    for(j=0; j<SM_NB_SEG; j++){
      s->TimeCumulM[i][j] = e->TimeCumulM[i][j];
      s->TimeCumul[i][j] = e->TimeCumul[i][j];
    }
  }
  return;
}



void sm_copy_SM_MOTION_MONO_into(const SM_MOTION_MONO* e, SM_MOTION_MONO* s)
{
  int j=0;

  sm_SM_TIMES_copy_into(&(e->Times), &(s->Times));
  sm_SM_TIMES_copy_into(&(e->TimesM), &(s->TimesM));
  sm_SM_TIMES_copy_into(&(e->Acc), &(s->Acc));
  sm_SM_TIMES_copy_into(&(e->Vel), &(s->Vel));
  sm_SM_TIMES_copy_into(&(e->Pos), &(s->Pos));
  s->jerk.sel = e->jerk.sel;
  s->jerk.J1 = e->jerk.J1;
  s->jerk.J2 = e->jerk.J2;
  s->jerk.J3 = e->jerk.J3;
  s->jerk.J4 = e->jerk.J4;
  s->IC.a = e->IC.a;
  s->IC.v = e->IC.v;
  s->IC.x = e->IC.x;
  s->FC.a = e->FC.a;
  s->FC.v = e->FC.v;
  s->FC.x = e->FC.x;
  s->Dir = e->Dir;
  s->Dir_a = e->Dir_a;
  s->Dir_b = e->Dir_b;
  s->MotionDuration = e->MotionDuration;
  s->MotionDurationM = e->MotionDurationM;
  s->motionIsAdjusted = e->motionIsAdjusted;
  for(j=0; j<SM_NB_SEG; j++){
    s->TimeCumulM[j] = e->TimeCumulM[j];
    s->TimeCumul[j] = e->TimeCumul[j];
  }

  return;
}

// FUNCTIONS FOR POINT TO POINT MOTION METHOD OF NACHO
/* Jerk Profile
   tpJerkProfile(double Dist, double Jmax, double Amax, double Vmax, int Dir, double *Tj, double *Tac, double *Tvc, double *TimeL);

   Inputs:   Dist       Distance
   Jmax       Maximum Jerk
   Amax       Maximum Acceleration
   Vmax       Maximum Velocity
   Dir        Direction

   Outputs:  Tj         Jerk Time
   Tac        Acceleration Constant Time
   Tvc        Velocity Constant Time

   Return:   TP_OK
   TP_ERROR      - Distance Negative
*/
SM_STATUS sm_JerkProfilePointToPoint(double Dist, double Jmax, double Amax, double Vmax, double *Tj, double *Tac, double *Tvc, double *TimeL)
{
  double Jmax2 = Jmax*Jmax;
  double Amax2 = Amax*Amax;
  double Amax3 = Amax*Amax2;
  double Vmax2 = Vmax*Vmax;
  double Ta;
  double Thr1, Thr2;
  double Xdif, Xjp, Xjn;

  /* Temps Initialitation */
  *Tj  = 0.0;
  *Tac = 0.0;
  *Tvc = 0.0;
  *TimeL = 0.0;
  if (Dist == 0) return SM_OK;
  if (Dist < 0) {
    fprintf(stderr," sm_JerkProfile - SM_ERROR - Distance negative \n");
    return SM_ERROR;
  }

  /* Acceleration Temps Calcul */
  *Tj  = Amax/Jmax;
  *Tac = Vmax/Amax - Amax/Jmax;
  Ta   =*Tac +  2*(*Tj);

  /* Calcul of Threshold 1 */
  Thr1 = Amax*Vmax/Jmax + Vmax2/Amax;

  /*  There are three main cases
      Case 1 : Distance = Threshold
      Case 2 : Distance > Threshold
      Case 3 : Distance < Threshold
      Calcul of a new Threshold (Threshold2)
      Case 3.1 : Distance = Threshold2
      Case 3.2 : Distance > Threshold2
      Case 3.3 : Distance < Threshold2  */

  if (Dist != Thr1) {
    if (Dist > Thr1) {
      /* Case 2 */
      Xdif = Dist - Thr1;
      *Tvc = Xdif/Vmax;
    }
    else {
      /* Case 3 */
      Xjp  = Amax3/Jmax2/6;    /* Distance with Positive Jerk */
      Xjn  = 5*Xjp;            /* Distance with Negative Jerk */
      /* Threshold 2 Calcul */
      Thr2 = Xjp + Xjn;
      if (Dist == Thr2) {
	/* Case 3.1 */
	*Tac = 0.0;
      }
      else {
	if (Dist/2 > Thr2) {
	  /* Case 3.2 */
	  Xdif = Dist/2 - Thr2;
	  *Tac = SQRT(0.25*Amax2/Jmax2 + Dist/Amax)-1.5*Amax/Jmax;
	}
	else {
	  *Tac = 0.0;
	  *Tj = cbrt(0.5*Dist/Jmax);
	}
      }
    }
  }
  *TimeL = 4*(*Tj) + 2*(*Tac) + *Tvc;
  return SM_OK;
}


/* Like tpJerkProfileVectors but this function doesn't calcule the vectors Acc, Vel and Pos */
SM_STATUS sm_JerkProfileAdjustedPointToPoint(double GD,SM_COND IC, SM_TIMES Times, SM_TIMES TM, int Dir, SM_LIMITS Limits, SM_JERKS* Jmax, SM_TIMES* Acc, SM_TIMES* Vel, SM_TIMES* Pos)
{
  double J1;
  double Tjc = Times.Tjpa;
  double Tac = Times.Taca;
  double Tvc = Times.Tvc;
  double Tjc2 = Tjc*Tjc;
  double Tjc3 = Tjc*Tjc2;
  double Tac2 = Tac*Tac;
  //  SM_JERKS Jmax;
  //  SM_TIMES Acc, Vel, Pos;
  SM_COND FC;

  /*	CASE 1:   Tvc         Tac         Tjp
    CASE 2:   Tvc=0       Tac         Tjp
    CASE 3:   Tvc=0       Tac=0       Tjp
  */
  if (Tvc!=0)
    J1 = GD/(Tjc*(2*Tjc2+3*Tjc*Tac+Tjc*Tvc+Tac*Tvc+Tac2));
  else
    if (Tac!=0)
      J1 = GD/(Tjc*(2*Tjc2+3*Tjc*Tac+Tac2));
    else
      J1 = 0.5*GD/Tjc3;



  if (isinf(J1)) {
    printf("Jerk = inf GD= %f\n",GD);
    J1 = 0.0;
    return SM_ERROR;
  }
  if (isnan(J1)) {
    printf("Jerk = nan GD= %f\n",GD);
    J1 = 0.0;
    return SM_ERROR;
  }

  Jmax->sel = 1;
  Jmax->J1  = J1;
  Jmax->J2  = J1;
  Jmax->J3  = J1;
  Jmax->J4  = J1;

  if (sm_VerifyTimes(SM_DISTANCE_TOLERANCE_LINEAR, GD, *Jmax, IC, Dir, Times, &FC, Acc, Vel, Pos, SM_ON)!= 0) {
    fprintf(stderr,"sm_JerkProfileAdjusted - SM_ERROR - Verify Times \n");
    return SM_ERROR;
  }
  return SM_OK;
}


SM_STATUS sm_GeometricalCalculationsPointToPoint(double PFrom[SM_NB_DIM], double PTo[SM_NB_DIM],	double *MaxLDist,  double *MaxADist, double Dist[SM_NB_DIM], double AbsDist[SM_NB_DIM], int Dir[SM_NB_DIM])
{
  int i=0;
  *MaxLDist = 0.0;
  *MaxADist = 0.0;

  for (i=0; i<SM_NB_DIM; i++) {
    /* Distance Calcul */
    Dist[i] = PTo[i] - PFrom[i];
    /* Find Absolut Distance Deplacement */
    AbsDist[i] = ABS(Dist[i]);
    /* Calcul Direction Vector */
    Dir[i] = SIGN(Dist[i]);
  }
  for (i=0; i< 3; i++) {
    if(AbsDist[i] > *MaxLDist) {
      *MaxLDist = AbsDist[i];
    }
  }
  for (i=3; i<SM_NB_DIM ; i++) {
    if(AbsDist[i] > *MaxADist) {
      *MaxADist = AbsDist[i];
    }
  }
  return SM_OK;
}

SM_STATUS sm_CalculPointToPointJerkProfile(double PFrom[SM_NB_DIM], SM_POSELIMITS Limits, double MaxLDist, double MaxADist, double MaxDist, double AbsDist[SM_NB_DIM], int Dir[SM_NB_DIM], SM_JERKS jerk[], SM_TIMES Acc[], SM_TIMES Vel[], SM_TIMES Pos[], SM_TIMES* TNE_sec, SM_TIMES* TNE)
{
  double Tjc, Tac, Tvc;
  SM_JERKS Jerks;
  SM_TIMES Times, TM;
  SM_COND IC, FC;
  double LTjc, LTac, LTvc, TimeLL;
  double ATjc, ATac, ATvc, TimeAL;
  int NOE;
  SM_LIMITS auxLimits;
  int i=0;

  /* Gets Tjc, Tac and Tvc for Maximum Linear Distance Deplacement */
  if ( sm_JerkProfilePointToPoint(MaxLDist, Limits.linear.maxJerk, Limits.linear.maxAcc, Limits.linear.maxVel, &LTjc, &LTac, &LTvc, &TimeLL)!=0) {
    printf(" jerk Profile Gets Times for Maximum Linear Distance");
    return SM_ERROR;
  }
  /* Gets Tjc, Tac and Tvc for Maximum Angular Distance Deplacement */
  if ( sm_JerkProfilePointToPoint(MaxADist, Limits.angular.maxJerk, Limits.angular.maxAcc, Limits.angular.maxVel, &ATjc, &ATac, &ATvc, &TimeAL)!=0) {
    printf(" jerk Profile  Gets Times for Maximum Angular Distance");
    return SM_ERROR;
  }
  if (TimeAL <= TimeLL) {
    Tjc = LTjc;
    Tac = LTac;
    Tvc = LTvc;
    MaxDist = MaxLDist;
    Jerks.sel = 1;
    Jerks.J1 = Limits.linear.maxJerk;
  } else {
    Tjc = ATjc;
    Tac = ATac;
    Tvc = ATvc;
    MaxDist = MaxADist;
    Jerks.sel = 1;
    Jerks.J1 = Limits.angular.maxJerk;
  }
  /* Times Structure T construction */
  Times.Tjpa = Tjc;
  Times.Taca = Tac;
  Times.Tjna = Tjc;
  Times.Tvc  = Tvc;
  Times.Tjnb = Tjc;
  Times.Tacb = Tac;
  Times.Tjpb = Tjc;

  /* Initial Conditions for Verify Times */
  IC.a = 0.0;
  IC.v = 0.0;
  IC.x = 0.0;

  /* Verify Times */
  if (sm_VerifyTimes(SM_DISTANCE_TOLERANCE_LINEAR, MaxDist, Jerks, IC, 1, Times, &FC, &Acc[0], &Vel[0], &Pos[0], SM_ON)!=0) {
    printf(" Verify Times  1");
    printf("DISTANCE_TOLERANCE %f\n",SM_DISTANCE_TOLERANCE_LINEAR);
    printf("MaxDist %f\n",MaxDist);
    printf("Jerks.J1 %f\n",Jerks.J1);
    return SM_ERROR;
  }

  /* Get Number of Elements needed for Vectors Definition */
  if (sm_GetMonotonicTimes(Times, &TM, &NOE)!=0) {
    printf(" Get MonotonicTimes ");
    return SM_ERROR;
  }
  sm_GetNumberOfElement (&TM, TNE);

  for(i=0; i<SM_NB_DIM; i++) {

    if(i<3) {
      auxLimits.maxJerk = Limits.linear.maxJerk;
      auxLimits.maxAcc  = Limits.linear.maxVel;
      auxLimits.maxVel  = Limits.linear.maxAcc;
    } else {
      auxLimits.maxJerk = Limits.angular.maxJerk;
      auxLimits.maxAcc  = Limits.angular.maxVel;
      auxLimits.maxVel  = Limits.angular.maxAcc;
    }

    IC.a=0;
    IC.v=0;
    IC.x=PFrom[i];
    if (sm_JerkProfileAdjustedPointToPoint(AbsDist[i], IC, Times, TM, Dir[i], auxLimits, &jerk[i], &Acc[i], &Vel[i], &Pos[i])!= 0) {
      printf("  Profile Vectors on %d absdist = %f",i,AbsDist[i]) ;
      printf("Tjpa %f Taca %f Tjna %f Tvc %f Tjnb %f Tacb %f Tjpb %f\n",Times.Tjpa,Times.Taca,Times.Tjna,Times.Tvc,Times.Tjnb,Times.Tacb,Times.Tjpb);
      return SM_ERROR;
    }
  }

  sm_SM_TIMES_copy_into(&TM, TNE_sec);
  return SM_OK;
}


SM_STATUS sm_ComputeSoftMotionPointToPoint(SM_COND IC[SM_NB_DIM], SM_COND FC[SM_NB_DIM], SM_POSELIMITS Limits, SM_MOTION *motion) {


  double PFrom[SM_NB_DIM];
  double PTo[SM_NB_DIM];
  double MaxLDist= 0.0;
  double MaxADist= 0.0;
  double MaxDist = 0.0;
  double Dist[SM_NB_DIM];
  double AbsDist[SM_NB_DIM];
  int Dir[SM_NB_DIM];
  SM_JERKS jerk[SM_NB_DIM];
  SM_TIMES Acc[SM_NB_DIM];
  SM_TIMES Vel[SM_NB_DIM];
  SM_TIMES Pos[SM_NB_DIM];
  SM_TIMES TNE, TNE_sec;
  int i=0;
  double sum, sum_sec;

  /*Init From and To */
  PFrom[0] = IC[0].x;
  PFrom[1] = IC[1].x;
  PFrom[2] = IC[2].x;
  PFrom[3] = IC[3].x;
  PFrom[4] = IC[4].x;
  PFrom[5] = IC[5].x;

  /*Init To and To */
  PTo[0] = FC[0].x;
  PTo[1] = FC[1].x;
  PTo[2] = FC[2].x;
  PTo[3] = FC[3].x;
  PTo[4] = FC[4].x;
  PTo[5] = FC[5].x;

  if(sm_GeometricalCalculationsPointToPoint(PFrom, PTo, &MaxLDist, &MaxADist, Dist, AbsDist, Dir)!=0) {
    printf("ERROR sm_GeometricalCalculations\n");
    return SM_ERROR;
  }

  if(sm_CalculPointToPointJerkProfile(PFrom, Limits, MaxLDist, MaxADist, MaxDist, AbsDist, Dir, jerk, Acc, Vel, Pos, &TNE_sec, &TNE)!=0) {
    printf("ERROR sm_CalculPointToPointJerkProfile\n");
    return SM_ERROR;
  }


  sm_sum_motionTimes(&TNE, &sum);
  sm_sum_motionTimes(&TNE_sec, &sum_sec);

  for(i = 0; i<SM_NB_DIM; i++) {
    if(isinf(jerk[i].J1)) {
      printf("isinf(jerk[i].J1)\n");
    }
    if(isnan(jerk[i].J1)) {
      printf("isinf(jerk[i].J1)\n");
    }
    sm_SM_TIMES_copy_into(&TNE_sec, &(motion->Times[i]));
    sm_SM_TIMES_copy_into(&TNE, &(motion->TNE));
    sm_SM_TIMES_copy_into(&TNE, &(motion->TimesM[i]));
    sm_SM_TIMES_copy_into(&Acc[i], &(motion->Acc[i]));
    sm_SM_TIMES_copy_into(&Vel[i], &(motion->Vel[i]));
    sm_SM_TIMES_copy_into(&Pos[i], &(motion->Pos[i]));
    motion->jerk[i].sel = jerk[i].sel;
    motion->jerk[i].J1 = jerk[i].J1;
    motion->jerk[i].J2 = jerk[i].J2;
    motion->jerk[i].J3 = jerk[i].J3;
    motion->jerk[i].J4 = jerk[i].J4;
    motion->Dir[i] = Dir[i];
    motion->Dir_a[i] = Dir[i];
    motion->Dir_b[i] = -Dir[i];
    motion->motionIsAdjusted[i] = 0;
    motion->MotionDurationM[i] = sum;
    motion->MotionDuration[i] = sum_sec;

    motion->TimeCumulM[i][0] = 0;
    motion->TimeCumulM[i][1] = (int)motion->TimesM[i].Tjpa;
    motion->TimeCumulM[i][2] = motion->TimeCumulM[i][1] + (int)motion->TimesM[i].Taca;
    motion->TimeCumulM[i][3] = motion->TimeCumulM[i][2] + (int)motion->TimesM[i].Tjna;
    motion->TimeCumulM[i][4] = motion->TimeCumulM[i][3] + (int)motion->TimesM[i].Tvc;
    motion->TimeCumulM[i][5] = motion->TimeCumulM[i][4] + (int)motion->TimesM[i].Tjnb;
    motion->TimeCumulM[i][6] = motion->TimeCumulM[i][5] + (int)motion->TimesM[i].Tacb;

    motion->TimeCumul[i][0] = 0;
    motion->TimeCumul[i][1] = motion->Times[i].Tjpa;
    motion->TimeCumul[i][2] = motion->TimeCumul[i][1] + motion->Times[i].Taca;
    motion->TimeCumul[i][3] = motion->TimeCumul[i][2] + motion->Times[i].Tjna;
    motion->TimeCumul[i][4] = motion->TimeCumul[i][3] + motion->Times[i].Tvc;
    motion->TimeCumul[i][5] = motion->TimeCumul[i][4] + motion->Times[i].Tjnb;
    motion->TimeCumul[i][6] = motion->TimeCumul[i][5] + motion->Times[i].Tacb;
  }
  return SM_OK;
}


SM_STATUS sm_ComputeSoftMotionPointToPoint_gen(int nbAxis,double* J_max, double *A_max, double *V_max, SM_MOTION_MONO motion[]) {


  double MaxDist = 0.0;
  //  double MaxLDist = 0.0;
  int MaxLAxis = 0;
  double LTjc = 0.0, LTac = 0.0, LTvc = 0.0, TimeLi = 0.0, TimeL = 0.0;
  double Tjc = 0.0, Tac = 0.0, Tvc = 0.0;
  SM_LIMITS auxLimits;
  int i=0;
  SM_TIMES TNE, TNE_sec;
  double sum, sum_sec;
  int NOE;

  double *PFrom = NULL;
  double *PTo   = NULL;
  double *Dist  = NULL;
  double *AbsDist = NULL;
  int *Dir = NULL;
  SM_JERKS *jerk = NULL;
  SM_TIMES *Acc  = NULL;
  SM_TIMES *Vel  = NULL;
  SM_TIMES *Pos  = NULL;

  SM_JERKS Jerks;
  SM_TIMES Times, TM;
  SM_COND IC, FC;

  Jerks.sel = 1;
  Jerks.J1 = 0.0;
  Jerks.J2 = 0.0;
  Jerks.J3 = 0.0;
  Jerks.J4 = 0.0;

  if ((PFrom = MY_ALLOC(double, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((PTo = MY_ALLOC(double, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((Dist = MY_ALLOC(double, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((AbsDist = MY_ALLOC(double, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((Dir = MY_ALLOC(int, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((jerk = MY_ALLOC(SM_JERKS, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((Acc = MY_ALLOC(SM_TIMES, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((Vel = MY_ALLOC(SM_TIMES, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }
  if ((Pos = MY_ALLOC(SM_TIMES, nbAxis)) == NULL) {
    printf("  lm_create_softMotion: allocation failed\n");
    return SM_ERROR;
  }

  TimeL = 0.0;
  for(i = 0; i<nbAxis; i++) {
    PFrom[i] = motion[i].IC.x;
    PTo[i] = motion[i].FC.x;
    /* Distance Calcul */
    Dist[i] = PTo[i] - PFrom[i];
    /* Find Absolut Distance Deplacement */
    AbsDist[i] = ABS(Dist[i]);
    /* Calcul Direction Vector */
    Dir[i] = SIGN(Dist[i]);
    // 		if(AbsDist[i] > MaxLDist) {
    // 			MaxLDist = AbsDist[i];
    // 			MaxLAxis = i;
    // 		}

    if ( sm_JerkProfilePointToPoint(AbsDist[i] , J_max[i], A_max[i], V_max[i], &LTjc, &LTac, &LTvc, &TimeLi)!=0) {
      printf(" jerk Profile Gets Times for Maximum Linear Distance");
      return SM_ERROR;
    }
    if (TimeLi >= TimeL) {
      Tjc = LTjc;
      Tac = LTac;
      Tvc = LTvc;
      MaxDist = AbsDist[i];
      Jerks.sel = 1;
      MaxLAxis = i;
      Jerks.J1 = J_max[i];
      TimeL = TimeLi;
    }
  }

  /* Times Structure T construction */
  Times.Tjpa = Tjc;
  Times.Taca = Tac;
  Times.Tjna = Tjc;
  Times.Tvc  = Tvc;
  Times.Tjnb = Tjc;
  Times.Tacb = Tac;
  Times.Tjpb = Tjc;

  /* Initial Conditions for Verify Times */
  IC.a = 0.0;
  IC.v = 0.0;
  IC.x = 0.0;

  /* Verify Times */
  if (sm_VerifyTimes(SM_DISTANCE_TOLERANCE_LINEAR, MaxDist, Jerks, IC, 1, Times, &FC, &Acc[0], &Vel[0], &Pos[0], SM_ON)!=0) {
    printf(" Verify Times  1");
    printf("DISTANCE_TOLERANCE %f\n",SM_DISTANCE_TOLERANCE_LINEAR);
    printf("MaxDist %f\n",MaxDist);
    printf("Jerks.J1 %f\n",Jerks.J1);
    return SM_ERROR;
  }

  /* Get Number of Elements needed for Vectors Definition */
  if (sm_GetMonotonicTimes(Times, &TM, &NOE)!=0) {
    printf(" Get MonotonicTimes ");
    return SM_ERROR;
  }
  sm_GetNumberOfElement (&TM, &TNE);

  for(i=0; i<nbAxis; i++) {

    auxLimits.maxJerk = J_max[i];
    auxLimits.maxAcc  = A_max[i];
    auxLimits.maxVel  = V_max[i];
    IC.a=0.0;
    IC.v=0.0;
    IC.x=PFrom[i];
    if (sm_JerkProfileAdjustedPointToPoint(AbsDist[i], IC, Times, TM, Dir[i], auxLimits, &jerk[i], &Acc[i], &Vel[i], &Pos[i])!= 0) {
      printf("  Profile Vectors on %d absdist = %f",i,AbsDist[i]) ;
      printf("Tjpa %f Taca %f Tjna %f Tvc %f Tjnb %f Tacb %f Tjpb %f\n",Times.Tjpa,Times.Taca,Times.Tjna,Times.Tvc,Times.Tjnb,Times.Tacb,Times.Tjpb);
      return SM_ERROR;
    }
  }
  // IS HERE !!!!!
  sm_SM_TIMES_copy_into(&Times, &TNE_sec);
  //sm_SM_TIMES_copy_into(&TM, &TNE_sec);

  sm_sum_motionTimes(&TNE, &sum);
  sm_sum_motionTimes(&TNE_sec, &sum_sec);

  for( i = 0; i<nbAxis; i++) {
    if(isinf(jerk[i].J1)) {
      printf("isinf(jerk[i].J1)\n");
    }
    if(isnan(jerk[i].J1)) {
      printf("isinf(jerk[i].J1)\n");
    }

    sm_SM_TIMES_copy_into(&Times, &(motion[i].Times));
    //sm_SM_TIMES_copy_into(&TNE_sec, &(motion[i].Times));

    sm_SM_TIMES_copy_into(&TNE, &(motion[i].TimesM));
    sm_SM_TIMES_copy_into(&Acc[i], &(motion[i].Acc));
    sm_SM_TIMES_copy_into(&Vel[i], &(motion[i].Vel));
    sm_SM_TIMES_copy_into(&Pos[i], &(motion[i].Pos));
    motion[i].jerk.sel = jerk[i].sel;
    motion[i].jerk.J1 = jerk[i].J1;
    motion[i].jerk.J2 = jerk[i].J2;
    motion[i].jerk.J3 = jerk[i].J3;
    motion[i].jerk.J4 = jerk[i].J4;
    motion[i].Dir = Dir[i];
    motion[i].Dir_a = Dir[i];
    motion[i].Dir_b = -Dir[i];
    motion[i].motionIsAdjusted = 0;
    motion[i].MotionDurationM = sum;
    motion[i].MotionDuration = sum_sec;

    motion[i].TimeCumulM[0] = 0;
    motion[i].TimeCumulM[1] = (int)motion[i].TimesM.Tjpa;
    motion[i].TimeCumulM[2] = motion[i].TimeCumulM[1] + (int)motion[i].TimesM.Taca;
    motion[i].TimeCumulM[3] = motion[i].TimeCumulM[2] + (int)motion[i].TimesM.Tjna;
    motion[i].TimeCumulM[4] = motion[i].TimeCumulM[3] + (int)motion[i].TimesM.Tvc;
    motion[i].TimeCumulM[5] = motion[i].TimeCumulM[4] + (int)motion[i].TimesM.Tjnb;
    motion[i].TimeCumulM[6] = motion[i].TimeCumulM[5] + (int)motion[i].TimesM.Tacb;

    motion[i].TimeCumul[0] = 0;
    motion[i].TimeCumul[1] = motion[i].Times.Tjpa;
    motion[i].TimeCumul[2] = motion[i].TimeCumul[1] + motion[i].Times.Taca;
    motion[i].TimeCumul[3] = motion[i].TimeCumul[2] + motion[i].Times.Tjna;
    motion[i].TimeCumul[4] = motion[i].TimeCumul[3] + motion[i].Times.Tvc;
    motion[i].TimeCumul[5] = motion[i].TimeCumul[4] + motion[i].Times.Tjnb;
    motion[i].TimeCumul[6] = motion[i].TimeCumul[5] + motion[i].Times.Tacb;
  }

  MY_FREE(PFrom, double, nbAxis);
  MY_FREE(PTo, double, nbAxis);
  MY_FREE(Dist, double, nbAxis);
  MY_FREE(AbsDist, double, nbAxis);
  MY_FREE(Dir, int, nbAxis);
  MY_FREE(jerk, SM_JERKS, nbAxis);
  MY_FREE(Acc, SM_TIMES, nbAxis);
  MY_FREE(Vel, SM_TIMES, nbAxis);
  MY_FREE(Pos, SM_TIMES, nbAxis);

  return SM_OK;
}



SM_STATUS sm_adjustMotionWith3seg( SM_COND IC, SM_COND FC, double Timp, SM_MOTION_MONO  *motion){
  /* This funciton compute the motion using 3 segment method without any optimization
     -- IC   : Initial condition
     -- FC   : Final condition
     -- Timp : Imposed time
     -- motion : output motion stored in a structure
  */
  double locRHS[3]; //partie droite du system
  double T1, T2, T3;
  double J1, J2, J3;
  int NOE;
  int dir1 , dir2, dir3;
  double distanceTolerance = 0.0001;
  SM_TIMES smTimesTmp;

  locRHS[0] = (FC.a - IC.a) / Timp;
  locRHS[1] = (FC.v - IC.v - IC.a * Timp) / pow(Timp,2.0);
  locRHS[2] = (FC.x - IC.x - IC.a * pow(Timp,2.0)/2.0 - IC.v * Timp) / pow(Timp,3.0);

  J1 =  1.0 * locRHS[0] -  9.0 * locRHS[1]  + 27.0 * locRHS[2];
  J2 = -3.5 * locRHS[0] + 27.0 * locRHS[1]  - 54.0 * locRHS[2];
  J3 =  5.5 * locRHS[0] - 18.0 * locRHS[1]  + 27.0 * locRHS[2];

  dir1 = SIGN(J1);
  dir2 = SIGN(J2);
  dir3 = SIGN(J3);

  T1 = Timp / 3.0;
  T2 = Timp / 3.0;
  T3 = Timp / 3.0;

  motion->jerk.sel = 4;
  motion->jerk.J1 = J1;
  if (J2 > 0.0) {
    motion->jerk.J2 = -fabs(J2);
  } else {
    motion->jerk.J2 = fabs(J2);
  }
  motion->jerk.J3 = J3;
  motion->jerk.J4 = 0.0;
  motion->Dir = 1;
  motion->Dir_a = 1;
  motion->Dir_b = 1;

  motion->IC.a = IC.a;
  motion->IC.v = IC.v;
  motion->IC.x = IC.x;
  motion->FC.a = FC.a;
  motion->FC.v = FC.v;
  motion->FC.x = FC.x;

  motion->motionIsAdjusted = 1;
  motion->Times.Tjpa = T1;
  motion->Times.Taca = 0.0;
  motion->Times.Tjna = T2;
  motion->Times.Tvc  = 0.0;
  motion->Times.Tjnb = T3;
  motion->Times.Tacb = 0.0;
  motion->Times.Tjpb = 0.0;

  sm_GetMonotonicTimes(motion->Times, &smTimesTmp, &NOE);
  sm_GetNumberOfElement(&smTimesTmp, &motion->TimesM);

  sm_sum_motionTimes(&(motion->Times), &(motion->MotionDuration));
  sm_sum_motionTimes(&(motion->TimesM), &(motion->MotionDurationM));


  motion->TimeCumulM[0] = 0;
  motion->TimeCumulM[1] = (int)motion->TimesM.Tjpa;
  motion->TimeCumulM[2] = (int)motion->TimeCumulM[1] \
    + (int)motion->TimesM.Taca;
  motion->TimeCumulM[3] = (int)motion->TimeCumulM[2] \
    + (int)motion->TimesM.Tjna;
  motion->TimeCumulM[4] = (int)motion->TimeCumulM[3] \
    + (int)motion->TimesM.Tvc;
  motion->TimeCumulM[5] = (int)motion->TimeCumulM[4] \
    + (int)motion->TimesM.Tjnb;
  motion->TimeCumulM[6] = (int)motion->TimeCumulM[5] \
    + (int)motion->TimesM.Tacb;

  motion->TimeCumul[0] = 0.0;
  motion->TimeCumul[1] = motion->Times.Tjpa;
  motion->TimeCumul[2] = motion->TimeCumul[1] \
    + motion->Times.Taca;
  motion->TimeCumul[3] = motion->TimeCumul[2] \
    + motion->Times.Tjna;
  motion->TimeCumul[4] = motion->TimeCumul[3] \
    + motion->Times.Tvc;
  motion->TimeCumul[5] = motion->TimeCumul[4] \
    + motion->Times.Tjnb;
  motion->TimeCumul[6] = motion->TimeCumul[5] \
    + motion->Times.Tacb;


  /* Verify Times */
  if (sm_VerifyTimes_Dir_ab(distanceTolerance, FC.x, motion->jerk, IC,
			    motion->Dir_a, motion->Dir_b,
			    motion->Times, &FC, &(motion->Acc),
			    &(motion->Vel), &(motion->Pos)) != 0) {
    printf("lm_compute_softMotion_for_r6Arm ERROR Verify Times \n");
    return SM_ERROR;
  }

  return SM_OK;
}


////////////////////////////////////////
////////////////////////////////////////
///////////////////////////////////////
////////////////////////////////////////
////////////////////////////////////////:
///////////////////////////////////////:::



//         sm_AVX_TimeVar(ICloc, &Tloc, &Jloc, 1, &Tloc, 1, &aloc, &vloc, &xloc););

SM_STATUS sm_AVX_TimeVar( std::vector<double> IC,  std::vector<double> T, std::vector<double> J, std::vector<double> t, std::vector<double> &a, std::vector<double> &v, std::vector<double> &x){

  /* This function computes the position, acceleration, vel at a given time (t array with nbSample elements) given
     -- IC[3]: Array of initial condition in acceleration (IC[0]) velocity (IC[1]) position (IC[2])
     -- *T   : Array of time segment
     -- *J   : Arrar of Jerk in each segment
     -- nbSeg : number of segment
     -- *t : array of sample points (echantillonage) in time
     -- nbSample : length of *t
     -- *a : output array of computed acceleration
     -- *v : output array of computed velocity
     -- *x : output array of computed position
  */

  std::vector<double> Tac;
  std::vector<double> const0;
  std::vector<double> const1;
  std::vector<double> const2;
  std::vector<double> const3;
  unsigned int i, j,n ;
  double auxT;

  /*---------------------------Initialize-------------------*/
  // allocate memory for local variable

  Tac.resize(T.size());
  const0.resize(T.size());
  const1.resize(T.size());
  const2.resize(T.size());
  const3.resize(T.size());

  // Initilize accumulate time end segment constant
  for (i = 0; i < T.size(); i++){
    if (i == 0) Tac.at(i) = 0.0;
    else {
      Tac.at(i) = 0.0;
      for (j = 0; j < i; j ++){
	Tac.at(i) = Tac.at(i) + T.at(j);
      }
    }
    const0[i] = 0.0;
    const1[i] = 0.0;
    const2[i] = 0.0;
    const3[i] = 0.0;
  }
  // Initilize acceleration, velocity anh position
  a.resize(t.size());
  v.resize(t.size());
  x.resize(t.size());

  for (i = 0;  i < t.size(); i++){
    a.at(i) =  0.0;
    v.at(i) =  0.0;
    x.at(i) =  0.0;
  }

  /*------------------- Compute end segment constant ---------------------------------*/
  for ( n = 0; n < T.size(); n++){

    for ( i = 0; i < n; i ++) {
      const0[n] = const0[n] + J[i] * T[i];
      const1[n] = const1[n] + J[i] * pow(T[i],2.0) / 2.0;
      const2[n] = const2[n] + J[i] * pow(T[i],3.0) / 6.0;

      for (j = i + 1; j < n; j ++) {

	const1[n] = const1[n] + J[i] * T[i] * T[j];
	const2[n] = const2[n] + J[i] * T[i] * pow(T[j],2.0) / 2.0;
      }

    }

    for (j = 0; j < n; j++){
      const3[n] = const3[n] + const1[j] * T[j];
    }
  }

  /*------------------- Compute AVX ---------------------------------*/
  for (i = 0; i < t.size(); i++){

    // Find segment Index
    n = T.size() -1;
    while (t.at(i) < Tac[n]) { n = n - 1;}

    auxT = t.at(i) - Tac[n];

     a.at(i) = J[n] * auxT                + IC[0];
     v.at(i) = J[n] * pow(auxT,2.0) / 2.0 + IC[0] * ( t.at(i) )                + IC[1];
     x.at(i) = J[n] * pow(auxT,3.0) / 6.0 + IC[0] * pow(( t.at(i) ),2.0) / 2.0 + IC[1] * ( t.at(i) ) + IC[2];

     a.at(i) = a.at(i) + const0[n] ;
     v.at(i) = v.at(i) + const0[n] * auxT              + const1[n];
     x.at(i) = x.at(i) + const0[n] * pow(auxT,2.0) / 2 + const1[n] * auxT +  const2[n] + const3[n];

  }
  return SM_OK;
}


SM_STATUS sm_AVX_TimeVar( std::vector<SM_SEG> seg, std::vector<double> t, std::vector<double> &a, std::vector<double> &v, std::vector<double> &x){

  /* This function computes the position, acceleration, vel at a given time (t array with nbSample elements) given
     -- IC[3]: Array of initial condition in acceleration (IC[0]) velocity (IC[1]) position (IC[2])
     -- *T   : Array of time segment
     -- *J   : Arrar of Jerk in each segment
     -- nbSeg : number of segment
     -- *t : array of sample points (echantillonage) in time
     -- nbSample : length of *t
     -- *a : output array of computed acceleration
     -- *v : output array of computed velocity
     -- *x : output array of computed position
  */

  std::vector<double> Tac;
  std::vector<double> const0;
  std::vector<double> const1;
  std::vector<double> const2;
  std::vector<double> const3;
  unsigned int i, j,n ;
  double auxT;

  /*---------------------------Initialize-------------------*/
  // allocate memory for local variable

  Tac.resize(seg.size());
  const0.resize(seg.size());
  const1.resize(seg.size());
  const2.resize(seg.size());
  const3.resize(seg.size());

  // Initilize accumulate time end segment constant
  for (i = 0; i < seg.size(); i++){
    if (i == 0) Tac.at(i) = 0.0;
    else {
      Tac.at(i) = 0.0;
      for (j = 0; j < i; j ++){
	Tac.at(i) = Tac.at(i) + seg.at(j).time;
      }
    }
    const0[i] = 0.0;
    const1[i] = 0.0;
    const2[i] = 0.0;
    const3[i] = 0.0;
  }
  // Initilize acceleration, velocity anh position
  a.resize(t.size());
  v.resize(t.size());
  x.resize(t.size());

  for (i = 0;  i < t.size(); i++){
    a.at(i) =  0.0;
    v.at(i) =  0.0;
    x.at(i) =  0.0;
  }

  /*------------------- Compute end segment constant ---------------------------------*/
  for ( n = 0; n < seg.size(); n++){

    for ( i = 0; i < n; i ++) {
      const0[n] = const0[n] + seg[i].jerk * seg[i].time;
      const1[n] = const1[n] + seg[i].jerk * pow(seg[i].time,2.0) / 2.0;
      const2[n] = const2[n] + seg[i].jerk * pow(seg[i].time,3.0) / 6.0;

      for (j = i + 1; j < n; j ++) {

	const1[n] = const1[n] + seg[i].jerk * seg[i].time * seg[j].time;
	const2[n] = const2[n] + seg[i].jerk * seg[i].time * pow(seg[j].time,2.0) / 2.0;
      }

    }

    for (j = 0; j < n; j++){
      const3[n] = const3[n] + const1[j] * seg[j].time;
    }
  }

  /*------------------- Compute AVX ---------------------------------*/
  for (i = 0; i < t.size(); i++){

    // Find segment Index
    n = seg.size() -1;
    while (t.at(i) < Tac[n]) { n = n - 1;}

    auxT = t.at(i) - Tac[n];

     a.at(i) = seg[n].jerk * auxT                + seg[0].IC.a;
     v.at(i) = seg[n].jerk * pow(auxT,2.0) / 2.0 + seg[0].IC.a * ( t.at(i) )                + seg[0].IC.v;
     x.at(i) = seg[n].jerk * pow(auxT,3.0) / 6.0 + seg[0].IC.a * pow(( t.at(i) ),2.0) / 2.0 + seg[0].IC.v * ( t.at(i) ) + seg[0].IC.x;

     a.at(i) = a.at(i) + const0[n] ;
     v.at(i) = v.at(i) + const0[n] * auxT              + const1[n];
     x.at(i) = x.at(i) + const0[n] * pow(auxT,2.0) / 2 + const1[n] * auxT +  const2[n] + const3[n];

  }
  return SM_OK;
}


SM_STATUS sm_getMotionCond(const SM_MOTION_MONO* motion, double time, SM_COND *cond){

  std::vector<double> I(3);
  std::vector<double> T(SM_NB_SEG);
  std::vector<double> J(SM_NB_SEG);
  std::vector<double> t(1);
  std::vector<double> a(1);
  std::vector<double> v(1);
  std::vector<double> x(1);


  I[0] = motion->IC.a;
  I[1] = motion->IC.v;
  I[2] = motion->IC.x;

  if(motion->MotionDuration > 0.001) {
    T[0] = motion->Times.Tjpa;
    T[1] = motion->Times.Taca;
    T[2] = motion->Times.Tjna;
    T[3] = motion->Times.Tvc;
    T[4] = motion->Times.Tjnb;
    T[5] = motion->Times.Tacb;
    T[6] = motion->Times.Tjpb;
    
    J[0] =   motion->Dir*motion->jerk.J1;
    J[1] =   0.0;
    J[2] = - motion->Dir*motion->jerk.J1;
    J[3] =   0.0;
    J[4] = - motion->Dir*motion->jerk.J1;
    J[5] =   0.0;
    J[6] =   motion->Dir*motion->jerk.J1;
    
    t[0] = time;
    
    sm_AVX_TimeVar(I, T, J, t, a, v, x);
    cond->a = a[0];
    cond->v = v[0];
    cond->x = x[0];     
  } else {
    cond->a = motion->IC.a;
    cond->v = motion->IC.v;
    cond->x = motion->IC.x;  
  }
  return SM_OK;
}

SM_STATUS sm_ComputeCondition(std::vector<SM_CURVE_DATA> &IdealTraj,std::vector<kinPoint> &discPoint, std::vector<SM_COND_DIM> &IC, std::vector<SM_COND_DIM> &FC, std::vector<double> &Timp, std::vector<int>
&IntervIndex){

  /* This function divide the global time in interval, in which 3 Jerk values are computed
     -- IdealTraj[]   : Ideal Traj to follow
     -- nbPoints      : number of input sample points (echantillons) of IdealTraj
     -- nbInterval    : desired number of discretized intervals
     -- IC[]          : output array of initial condition at the beginning of interval
     -- FC[]          : output array of final condition at the end of interval
     -- Timp          : output array of time values of each interval
     -- IntervIndex[] : output index of interval extremeties
  */


/*
	la taille de IdealTraj---really big
	nbPoints = ((int) (total_time/tic)) + 1;
	IdealTraj.resize(nbPoints);
*/
  unsigned int i;
  unsigned int j;
    kinPoint kc;

    kc.kc.resize(IC[0].Axis.size());
  // Compute the discretization index to have an interpolation evenly distributed in time


// donc, si le size de IdealTraj est 1000, et le size de Timp est 40, donc interval est 25.
    if (Timp.size() > 1){
        IntervIndex[0]           = 0;
        IntervIndex[Timp.size()] = IdealTraj.size() -1;
        for (i = 1; i < Timp.size(); i++){
            IntervIndex[i] = i * ( (int) (IdealTraj.size()  / Timp.size() ))-1;
        }
    }
    else{
        IntervIndex[0]           = 0;
        IntervIndex[Timp.size()] = IdealTraj.size()-1;
    }

  // Assign the IC, FC, Timp
  for (i = 0; i < Timp.size(); i++){

    Timp.at(i) = IdealTraj.at(IntervIndex.at(i + 1)).t - IdealTraj.at(IntervIndex.at(i)).t;

// pour mettre les 40 valeurs dans IC et FC en 3 axes
    for (j = 0; j < IC[i].Axis.size(); j++){

        IC[i].Axis[j].a = IdealTraj[IntervIndex[i]].Acc[j];
        IC[i].Axis[j].v = IdealTraj[IntervIndex[i]].Vel[j];
        IC[i].Axis[j].x = IdealTraj[IntervIndex[i]].Pos[j];

        kc.kc[j].a = IdealTraj[IntervIndex[i]].Acc[j];
        kc.kc[j].v = IdealTraj[IntervIndex[i]].Vel[j];
        kc.kc[j].x = IdealTraj[IntervIndex[i]].Pos[j];

        FC[i].Axis[j].a = IdealTraj[IntervIndex[i + 1]].Acc[j];
        FC[i].Axis[j].v = IdealTraj[IntervIndex[i + 1]].Vel[j];
        FC[i].Axis[j].x = IdealTraj[IntervIndex[i + 1]].Pos[j];
    }

    kc.t = IdealTraj[IntervIndex[i]].t;
    discPoint.push_back(kc);
  }
  return SM_OK;
}



SM_STATUS sm_ComputeCondition(std::vector<SM_CURVE_DATA3> &IdealTraj,std::vector<kinPoint3> &discPoint, std::vector<SM_COND_DIM3> &IC, std::vector<SM_COND_DIM3> &FC, std::vector<double> &Timp, std::vector<int>
&IntervIndex){
	
  /* This function divide the global time in interval, in which 3 Jerk values are computed
     -- IdealTraj[]   : Ideal Traj to follow
     -- nbPoints      : number of input sample points (echantillons) of IdealTraj
     -- nbInterval    : desired number of discretized intervals
     -- IC[]          : output array of initial condition at the beginning of interval
     -- FC[]          : output array of final condition at the end of interval
     -- Timp          : output array of time values of each interval
     -- IntervIndex[] : output index of interval extremeties
  */
	

/*
	la taille de IdealTraj---really big
	nbPoints = ((int) (total_time/tic)) + 1;
	IdealTraj.resize(nbPoints);
*/
  unsigned int i;
  unsigned int j;
    kinPoint3 kc;
  // Compute the discretization index to have an interpolation evenly distributed in time

	
// donc, si le size de IdealTraj est 1000, et le size de Timp est 40, donc interval est 25.
    if (Timp.size() > 1){
        IntervIndex[0]           = 0;
        IntervIndex[Timp.size()] = IdealTraj.size() -1;
        for (i = 1; i < Timp.size(); i++){
            IntervIndex[i] = i * ( (int) (IdealTraj.size()  / Timp.size() ))-1;
        }
    }
    else{
        IntervIndex[0]           = 0;
        IntervIndex[Timp.size()] = IdealTraj.size()-1;
    }

  // Assign the IC, FC, Timp
  for (i = 0; i < Timp.size(); i++){
		
    Timp[i] = IdealTraj[IntervIndex[i + 1]].t - IdealTraj[IntervIndex[i]].t;		

// pour mettre les 40 valeurs dans IC et FC en 3 axes 
    for (j = 0; j < 3; j++){
			
        IC[i].Axis[j].a = IdealTraj[IntervIndex[i]].Acc[j];
        IC[i].Axis[j].v = IdealTraj[IntervIndex[i]].Vel[j];
        IC[i].Axis[j].x = IdealTraj[IntervIndex[i]].Pos[j];
        
        kc.kc[j].a = IdealTraj[IntervIndex[i]].Acc[j];
        kc.kc[j].v = IdealTraj[IntervIndex[i]].Vel[j];
        kc.kc[j].x = IdealTraj[IntervIndex[i]].Pos[j];	
        
        FC[i].Axis[j].a = IdealTraj[IntervIndex[i + 1]].Acc[j];
        FC[i].Axis[j].v = IdealTraj[IntervIndex[i + 1]].Vel[j];
        FC[i].Axis[j].x = IdealTraj[IntervIndex[i + 1]].Pos[j];		
    }	

    kc.t = IdealTraj[IntervIndex[i]].t;
    discPoint.push_back(kc);
  }	
  return SM_OK;	
}


//SM_STATUS sm_SolveWithoutOpt(std::vector<SM_COND_DIM> &IC, std::vector<SM_COND_DIM> &FC, std::vector<double> &Timp, std::vector<SM_OUTPUT> &motion){
//  /* This funciton compute the motion using 3 segment method without any optimization
//     -- IC[]   : Initial condition array ( already discretized)
//     -- FC[]   : Final condition array ( already discretized)
//     -- Timp[] : Imposed time for each interval
//     -- nbIntervals : number of discretized intervals
//     -- motion[] : array of output command to the robot (composed of jerk and time duration for each axis)
//  */
//
//  double locRHS[3]; //partie droite du system
//  double ICloc[3];
//  double Tloc,Jloc;
//  double aloc, vloc, xloc;
//  int i, j;
//
//  // Compute jerk and time segment
//  for (i = 0; i < Timp.size(); i++){
//    for (j = 0; j < 3; j++){
//
//      locRHS[0] = (FC[i].Axis[j].a - IC[i].Axis[j].a) / Timp [i];
//      locRHS[1] = (FC[i].Axis[j].v - IC[i].Axis[j].v - IC[i].Axis[j].a * Timp[i]) / pow(Timp[i],2.0);
//      locRHS[2] = (FC[i].Axis[j].x - IC[i].Axis[j].x - IC[i].Axis[j].a * pow(Timp[i],2.0)/2.0 - IC[i].Axis[j].v * Timp[i]) / pow(Timp[i],3.0);
//
//      motion[i * 3 + 0].Jerk[j] =  1.0 * locRHS[0] -  9.0 * locRHS[1]  + 27.0 * locRHS[2];
//      motion[i * 3 + 1].Jerk[j] = -3.5 * locRHS[0] + 27.0 * locRHS[1]  - 54.0 * locRHS[2];
//      motion[i * 3 + 2].Jerk[j] =  5.5 * locRHS[0] - 18.0 * locRHS[1]  + 27.0 * locRHS[2];
//      SM_LOG(SM_LOG_DEBUG,  "J1 " << motion[i * 3 + 0].Jerk[j] << " J2 " <<motion[i * 3 + 1].Jerk[j] << " J3 " <<motion[i * 3 + 2].Jerk[j]);
//
//      motion[i * 3 + 0].Time[j] = Timp[i] / 3.0;
//      motion[i * 3 + 1].Time[j] = Timp[i] / 3.0;
//      motion[i * 3 + 2].Time[j] = Timp[i] / 3.0;
//
//      SM_LOG(SM_LOG_DEBUG,  "T1 " << motion[i * 3 + 0].Time[j] << " T2 " <<motion[i * 3 + 1].Time[j] << " T3 " <<motion[i * 3 + 2].Time[j]);
//    }
//  }
//  // Compute motion condition at the beginning of each segment
//  for (i = 0; i < 3; i++){
//
//    motion[0].IC[i].a = IC[0].Axis[i].a;
//    motion[0].IC[i].v = IC[0].Axis[i].v;
//    motion[0].IC[i].x = IC[0].Axis[i].x;
//    if(i==0) {
//      cout << " IC.a " << motion[0].IC[i].a << " IC.v " << motion[0].IC[i].v << " IC.x " << motion[0].IC[i].x << endl;
//    }
//  }
//
//  for (i = 1; i < (3 * Timp.size()); i++){
//
//    for (j = 0; j < 3; j++){
//
//      ICloc[0] = motion[i - 1].IC[j].a;
//      ICloc[1] = motion[i - 1].IC[j].v;
//      ICloc[2] = motion[i - 1].IC[j].x;
//
//      Tloc = motion[i-1].Time[j];
//      Jloc = motion[i-1].Jerk[j];
//
//      sm_AVX_TimeVar(ICloc, &Tloc, &Jloc, 1, &Tloc, 1, &aloc, &vloc, &xloc);
//
//      motion[i].IC[j].a = aloc;
//      motion[i].IC[j].v = vloc;
//      motion[i].IC[j].x = xloc;
//
//      ICloc[0] = motion[i].IC[j].a;
//      ICloc[1] = motion[i].IC[j].v;
//      ICloc[2] = motion[i].IC[j].x;
//
//      Tloc = motion[i].Time[j];
//      Jloc = motion[i].Jerk[j];
//
//      sm_AVX_TimeVar(ICloc, &Tloc, &Jloc, 1, &Tloc, 1, &aloc, &vloc, &xloc);
//
//
//
//      if(0){
//	cout << endl << "segement " << i << endl;
//	cout << " IC " << motion[i].IC[j].a << " " << motion[i].IC[j].v << " " << motion[i].IC[j].x << endl;
//	cout << " FC " << aloc << " " << vloc << " " << xloc << endl;
//	cout << endl;
//
//      }
//
//    }
//
//  }
//
//  return SM_OK;
//
//}

/*
  if (sm_SolveWithoutOpt(IC, FC, Timp, motion) != 0){
    printf("Solve Problem \n");
    return;
  }
*/

SM_STATUS sm_SolveWithoutOpt(std::vector<SM_COND_DIM3> &IC, std::vector<SM_COND_DIM3> &FC, std::vector<double> &Timp, std::vector<SM_OUTPUT> &motion){
  /* This funciton compute the motion using 3 segment method without any optimization
     -- IC[]   : Initial condition array ( already discretized)
     -- FC[]   : Final condition array ( already discretized)
     -- Timp[] : Imposed time for each interval
     -- nbIntervals : number of discretized intervals
     -- motion[] : array of output command to the robot (composed of jerk and time duration for each axis)
  */
	
  double locRHS[3]; //partie droite du system 
  std::vector<double>  ICloc;
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
  unsigned int i, j;

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(1);
  vloc.resize(1);
  xloc.resize(1);

  for(unsigned int k=0; k<motion.size(); k++) {
    motion.at(k).Jerk.resize(3);
    motion.at(k).Time.resize(3);
    motion.at(k).IC.resize(3);
  }

  // Compute jerk and time segment
  for (i = 0; i < IC.size(); i++){
    for (j = 0; j < 3; j++){
			
      locRHS[0] = (FC[i].Axis[j].a - IC[i].Axis[j].a) / Timp [i];
      locRHS[1] = (FC[i].Axis[j].v - IC[i].Axis[j].v - IC[i].Axis[j].a * Timp[i]) / pow(Timp[i],2.0);
      locRHS[2] = (FC[i].Axis[j].x - IC[i].Axis[j].x - IC[i].Axis[j].a * pow(Timp[i],2.0)/2.0 - IC[i].Axis[j].v * Timp[i]) / pow(Timp[i],3.0);
			
// pour les 40*3=120 segments, on calcul ses motion.Jerk, motion.Time, et motion.IC en 3 axis

// B * inv(A)
      motion[i * 3 + 0].Jerk[j] =  1.0 * locRHS[0] -  9.0 * locRHS[1]  + 27.0 * locRHS[2];
      motion[i * 3 + 1].Jerk[j] = -3.5 * locRHS[0] + 27.0 * locRHS[1]  - 54.0 * locRHS[2];
      motion[i * 3 + 2].Jerk[j] =  5.5 * locRHS[0] - 18.0 * locRHS[1]  + 27.0 * locRHS[2];
      SM_LOG(SM_LOG_DEBUG,  "J1 " << motion[i * 3 + 0].Jerk[j] << " J2 " <<motion[i * 3 + 1].Jerk[j] << " J3 " <<motion[i * 3 + 2].Jerk[j]);

      motion[i * 3 + 0].Time[j] = Timp[i] / 3.0;
      motion[i * 3 + 1].Time[j] = Timp[i] / 3.0;
      motion[i * 3 + 2].Time[j] = Timp[i] / 3.0;

      SM_LOG(SM_LOG_DEBUG,  "T1 " << motion[i * 3 + 0].Time[j] << " T2 " <<motion[i * 3 + 1].Time[j] << " T3 " <<motion[i * 3 + 2].Time[j]);
    }
  }

  // Compute motion condition at the beginning of each segment
  for (i = 0; i < 3; i++){
		
    motion[0].IC[i].a = IC[0].Axis[i].a; 
    motion[0].IC[i].v = IC[0].Axis[i].v; 
    motion[0].IC[i].x = IC[0].Axis[i].x; 
    //if(i==0) {
    //  cout << " IC.a " << motion[0].IC[i].a << " IC.v " << motion[0].IC[i].v << " IC.x " << motion[0].IC[i].x << endl;
    //}
  }
	
  for (i = 1; i < (3 * IC.size()); i++){
		
    for (j = 0; j < 3; j++){
			
      ICloc[0] = motion[i - 1].IC[j].a;
      ICloc[1] = motion[i - 1].IC[j].v;
      ICloc[2] = motion[i - 1].IC[j].x;
			
      Tloc.at(0) = motion[i-1].Time[j];
      Jloc.at(0) = motion[i-1].Jerk[j];
			
      sm_AVX_TimeVar(ICloc, Tloc, Jloc, Tloc, aloc, vloc, xloc);
			
      motion[i].IC[j].a = aloc.at(0);
      motion[i].IC[j].v = vloc.at(0);
      motion[i].IC[j].x = xloc.at(0);

      ICloc[0] = motion[i].IC[j].a;
      ICloc[1] = motion[i].IC[j].v;
      ICloc[2] = motion[i].IC[j].x;

      Tloc.at(0) = motion[i].Time[j];
      Jloc.at(0) = motion[i].Jerk[j];

      sm_AVX_TimeVar(ICloc, Tloc, Jloc, Tloc, aloc, vloc, xloc);

      SM_LOG(SM_LOG_DEBUG, endl << "segement " << i);
      SM_LOG(SM_LOG_DEBUG, " IC " << motion[i].IC[j].a << " " << motion[i].IC[j].v << " " << motion[i].IC[j].x);
      SM_LOG(SM_LOG_DEBUG, " FC " << aloc.at(0) << " " << vloc.at(0) << " " << xloc.at(0));
  
    }
	
  }

  return SM_OK;
	
}





SM_STATUS sm_SolveWithoutOpt(std::vector<SM_COND_DIM> &IC, std::vector<SM_COND_DIM> &FC, std::vector<double> &Timp, std::vector<SM_OUTPUT> &motion){
  /* This funciton compute the motion using 3 segment method without any optimization
     -- IC[]   : Initial condition array ( already discretized)
     -- FC[]   : Final condition array ( already discretized)
     -- Timp[] : Imposed time for each interval
     -- nbIntervals : number of discretized intervals
     -- motion[] : array of output command to the robot (composed of jerk and time duration for each axis)
  */

  double locRHS[3]; //partie droite du system
  std::vector<double>  ICloc;
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
  unsigned int i, j;

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(1);
  vloc.resize(1);
  xloc.resize(1);

  for(unsigned int k=0; k<motion.size(); k++) {
    motion.at(k).Jerk.resize(IC[0].Axis.size());
    motion.at(k).Time.resize(IC[0].Axis.size());
    motion.at(k).IC.resize(IC[0].Axis.size());
  }

  // Compute jerk and time segment
  for (i = 0; i < IC.size(); i++){ // IC.size toujours 1 donc i toujours 0
    for (j = 0; j < IC[0].Axis.size(); j++){

      locRHS[0] = (FC[i].Axis[j].a - IC[i].Axis[j].a) / Timp [i];
      locRHS[1] = (FC[i].Axis[j].v - IC[i].Axis[j].v - IC[i].Axis[j].a * Timp[i]) / pow(Timp[i],2.0);
      locRHS[2] = (FC[i].Axis[j].x - IC[i].Axis[j].x - IC[i].Axis[j].a * pow(Timp[i],2.0)/2.0 - IC[i].Axis[j].v * Timp[i]) / pow(Timp[i],3.0);

// pour les 40*3=120 segments, on calcul ses motion.Jerk, motion.Time, et motion.IC en 3 axis

// B * inv(A)
      motion[i * 3 + 0].Jerk[j] =  1.0 * locRHS[0] -  9.0 * locRHS[1]  + 27.0 * locRHS[2];
      motion[i * 3 + 1].Jerk[j] = -3.5 * locRHS[0] + 27.0 * locRHS[1]  - 54.0 * locRHS[2];
      motion[i * 3 + 2].Jerk[j] =  5.5 * locRHS[0] - 18.0 * locRHS[1]  + 27.0 * locRHS[2];
      SM_LOG(SM_LOG_DEBUG,  "J1 " << motion[i * 3 + 0].Jerk[j] << " J2 " <<motion[i * 3 + 1].Jerk[j] << " J3 " <<motion[i * 3 + 2].Jerk[j]);

      motion[i * 3 + 0].Time[j] = Timp[i] / 3.0;
      motion[i * 3 + 1].Time[j] = Timp[i] / 3.0;
      motion[i * 3 + 2].Time[j] = Timp[i] / 3.0;

      SM_LOG(SM_LOG_DEBUG,  "T1 " << motion[i * 3 + 0].Time[j] << " T2 " <<motion[i * 3 + 1].Time[j] << " T3 " <<motion[i * 3 + 2].Time[j]);
    }
  }

  // Compute motion condition at the beginning of each segment
  for (i = 0; i < IC[0].Axis.size(); i++){

    motion[0].IC[i].a = IC[0].Axis[i].a;
    motion[0].IC[i].v = IC[0].Axis[i].v;
    motion[0].IC[i].x = IC[0].Axis[i].x;
    //if(i==0) {
    //  cout << " IC.a " << motion[0].IC[i].a << " IC.v " << motion[0].IC[i].v << " IC.x " << motion[0].IC[i].x << endl;
    //}
  }

    SM_LOG(SM_LOG_DEBUG, endl << "segement " << 0);
    SM_LOG(SM_LOG_DEBUG, " IC " << motion[0].IC[0].a << " " << motion[0].IC[0].v << " " << motion[0].IC[0].x);
    SM_LOG(SM_LOG_DEBUG, " Jerk " << motion[0].Jerk[0] << " Time " << motion[0].Time[0]);
    //SM_LOG(SM_LOG_DEBUG, " FC " << aloc.at(0) << " " << vloc.at(0) << " " << xloc.at(0));

  for (i = 1; i < (3 * IC.size()); i++){

    for (j = 0; j < IC[0].Axis.size(); j++){

      ICloc[0] = motion[i - 1].IC[j].a;
      ICloc[1] = motion[i - 1].IC[j].v;
      ICloc[2] = motion[i - 1].IC[j].x;

      Tloc.at(0) = motion[i-1].Time[j];
      Jloc.at(0) = motion[i-1].Jerk[j];

      sm_AVX_TimeVar(ICloc, Tloc, Jloc, Tloc, aloc, vloc, xloc);

      motion[i].IC[j].a = aloc.at(0);
      motion[i].IC[j].v = vloc.at(0);
      motion[i].IC[j].x = xloc.at(0);

      ICloc[0] = motion[i].IC[j].a;
      ICloc[1] = motion[i].IC[j].v;
      ICloc[2] = motion[i].IC[j].x;

      Tloc.at(0) = motion[i].Time[j];
      Jloc.at(0) = motion[i].Jerk[j];

      sm_AVX_TimeVar(ICloc, Tloc, Jloc, Tloc, aloc, vloc, xloc);



     if(0){
	cout << endl << "segement " << i << endl;
	cout << " IC " << motion[i].IC[j].a << " " << motion[i].IC[j].v << " " << motion[i].IC[j].x << endl;
	cout << " FC " << aloc.at(0) << " " << vloc.at(0) << " " << xloc.at(0) << endl;
	cout << " Jerk " << motion[i].Jerk[j] << " Time " << motion[i].Time[j]<< endl;
	cout << endl;

      }

    }

  }

  return SM_OK;

}


SM_STATUS sm_InputScanning(char *fileName, int *nbLineArc, double *tic, int *nbIntervals, double PosInit[3], SM_LIMITS *Lim, SM_LINE_ARC comp[], SM_ROT rot[]){
  /* This function scan input file and return data needed to conpute the trajectory
     -- nbLineArc   : Number of lines and arcs that compose the path
     -- tic         : Discretization step in time
     -- nbIntervals : Number of discretization intervals
     -- PosInit     : Initial position of the path
     -- Lim         : Limits of sof motion
     -- comp[]      : Array of information of each component of the path
     -- rot[]       : Array of rotation matrix
  */

  FILE *ifp;
  char InputFileName[20];
  char *mode = (char*)"r";
  //  char myString[30];
  //double myValue;
  int i, j;

  int auxInt;
  char auxChar[20];
  char singleChar;
  char flagChar = '#';

  double X, Y, Z;
  double nx, ny, nz;
  double theta = 0.0, theta0 = 0.0;
  double aux0, aux1, aux2;
  double u[1000];
  double La, Lb, dL;
  double A;
  double sX, sY, sZ, cX, cY, cZ;
  double lastEnding[3];

  /*--------------Scan name and open input file ----------*/
  //printf("Enter input file name : ");
  //scanf("%19s",InputFileName);

  printf("Load %s trajectory definition file\n",fileName);
  ifp = fopen(fileName, mode);
  if (ifp == NULL) {
    fprintf(stderr, "Can not open %s file !\n",InputFileName);
    return SM_ERROR;
  }

  /*--------------Scan general parameters ----------*/
  fscanf(ifp,"%19s %19s %19s %19s %19s %19s", auxChar, auxChar, auxChar, auxChar, auxChar, auxChar);
  fscanf(ifp,"%d", nbLineArc);
  if (*nbLineArc > SM_NB_LINE_N_ARC) {
    printf("Too much Lines and arcs. Please reduce the number of components");
    return SM_ERROR;
  }
  fscanf(ifp,"%19s %19s", auxChar, auxChar);
  fscanf(ifp,"%lf", tic);
  fscanf(ifp,"%19s %19s %19s %19s", auxChar, auxChar, auxChar, auxChar);
  fscanf(ifp,"%d", nbIntervals);
  fscanf(ifp,"%19s %19s %19s %19s %19s", auxChar, auxChar, auxChar, auxChar, auxChar);
  fscanf(ifp,"%lf %19s %19s %lf %19s %19s %lf",&PosInit[0], auxChar, auxChar, &PosInit[1], auxChar, auxChar, &PosInit[2]);
  fscanf(ifp,"%19s %19s %19s %19s %19s", auxChar, auxChar, auxChar, auxChar, auxChar);
  fscanf(ifp,"%lf %19s %19s %lf %19s %19s %lf",&aux0, auxChar, auxChar, &aux1, auxChar, auxChar, &aux2);

  Lim->maxJerk = aux0;
  Lim->maxAcc  = aux1;
  Lim->maxVel  = aux2;
  printf("***********************************************\n");
  printf(" ==== trajectory parameters ===\n");
  printf("nbLineArc %d\n sampling time %lf\n nbIntervals %d\n x0 %lf y0 %lf z0 %lf\n Jmax %lf Amax %lf Vmax %lf\n", *nbLineArc, *tic, *nbIntervals, PosInit[0], PosInit[1],PosInit[2],Lim->maxJerk, Lim->maxAcc, Lim->maxVel);
  printf("***********************************************\n");
  printf(" ==== path description ===\n");
  /*--------------Scan Path data ----------*/

  // Initialize
  j = 0;
  lastEnding[0] = PosInit[0];
  lastEnding[1] = PosInit[1];
  lastEnding[2] = PosInit[2];

  while (!feof(ifp) && j < *nbLineArc){

    // Scan and find the starting of each line

    for (i = 0; i < 3000; i++){
      fscanf(ifp,"%c",&singleChar);
      if (singleChar == flagChar){
	break;
      }
      if (i ==2999){
	printf("Flag Not Scanned\n");
	return SM_ERROR;
      }
    }

    // Scan components information



    fscanf(ifp,"%d",&auxInt);
    fscanf(ifp,"%d",&auxInt);
    printf("%d  %d    \n",j + 1, auxInt);

    printf("beginning point of arc: x %f y %f z %f\n",  lastEnding[0], lastEnding[1] ,  lastEnding[2]);

    /* --------------------- If component is line, trasform line parameters to general clothoids parameters ----------------*/
    if (auxInt == 1){

      fscanf(ifp,"%lf %lf %lf",&X, &Y, &Z);
      printf("... load arc of type : Line\n");

      // convert line to general clothoid
      comp[j].invR0 = 0.0;
      comp[j].invRf = 0.0;
      comp[j].Ls    = sqrt(pow((X - lastEnding[0]),2.0) + pow((Y - lastEnding[1]),2.0) + pow((Z - lastEnding[2]),2.0));

      // update rotation matrix
      rot[j].R[0][0] = (X - lastEnding[0]) / comp[j].Ls;
      rot[j].R[1][0] = (Y - lastEnding[1]) / comp[j].Ls;
      rot[j].R[2][0] = (Z - lastEnding[2]) / comp[j].Ls;

      rot[j].R[0][1] = - rot[j].R[1][0] / sqrt(pow(rot[j].R[1][0],2.0) + pow(rot[j].R[0][0],2.0));
      rot[j].R[1][1] =   rot[j].R[0][0] / sqrt(pow(rot[j].R[1][0],2.0) + pow(rot[j].R[0][0],2.0));
      rot[j].R[2][1] = 0.0;

      rot[j].R[0][2] =    rot[j].R[1][0] * rot[j].R[2][1] - rot[j].R[2][0] * rot[j].R[1][1] ;
      rot[j].R[1][2] =    rot[j].R[2][0] * rot[j].R[0][1] - rot[j].R[0][0] * rot[j].R[2][1] ;
      rot[j].R[2][2] =    rot[j].R[0][0] * rot[j].R[1][1] - rot[j].R[1][0] * rot[j].R[0][1] ;

      // update ending point
      lastEnding[0] = X+ lastEnding[0] ;
      lastEnding[1] = Y+ lastEnding[1] ;
      lastEnding[2] = Z+ lastEnding[2] ;
      printf("ending point of current arc: x %f y %f z %f\n",  lastEnding[0], lastEnding[1] ,  lastEnding[2]);

    }
    /* --------------------- If component is circle, transform circle parameters to general clothoids parameters ----------------*/
    else if (auxInt == 2){

      //      fscanf(ifp,"%lf %lf %lf %lf %lf %lf %lf",&X, &Y, &Z, &nx, &ny, &nz, &theta, &theta0);
      fscanf(ifp,"%lf %lf %lf %lf %lf %lf %lf",&X, &Y, &Z, &nx, &ny, &nz, &theta);
      printf("... load arc of type : Circle\n");
      // convert line to general clothoid

      comp[j].invR0 = 1.0 / sqrt(pow((X - lastEnding[0]),2.0) + pow((Y - lastEnding[1]),2.0) + pow((Z - lastEnding[2]),2.0));
      comp[j].invRf = comp[j].invR0;
      comp[j].Ls    = abs(theta) / comp[j].invR0;
      printf("comp[j].invR0 %f comp[j].invRf %f comp[j].Ls %f\n",comp[j].invR0, comp[j].invRf, comp[j].Ls);

      // update rotation matrix
      rot[j].R[0][1] = (X - lastEnding[0]) * comp[j].invR0;
      rot[j].R[1][1] = (Y - lastEnding[1]) * comp[j].invR0;
      rot[j].R[2][1] = (Z - lastEnding[2]) * comp[j].invR0;

      rot[j].R[0][2] = nx / sqrt(pow(nx,2.0) + pow(ny,2.0) + pow(nz,2.0));
      rot[j].R[1][2] = ny / sqrt(pow(nx,2.0) + pow(ny,2.0) + pow(nz,2.0));
      rot[j].R[2][2] = nz / sqrt(pow(nx,2.0) + pow(ny,2.0) + pow(nz,2.0));

      rot[j].R[0][0] =    rot[j].R[1][1] * rot[j].R[2][2] - rot[j].R[2][1] * rot[j].R[1][2] ;
      rot[j].R[1][0] =    rot[j].R[2][1] * rot[j].R[0][2] - rot[j].R[0][1] * rot[j].R[2][2] ;
      rot[j].R[2][0] =    rot[j].R[0][1] * rot[j].R[1][2] - rot[j].R[1][1] * rot[j].R[0][2] ;

      // update ending point
      lastEnding[0] = rot[j].R[0][0] * sin(theta+theta0) / comp[j].invR0 +  rot[j].R[0][1] * (1.0 - cos(theta+theta0)) / comp[j].invR0 + lastEnding[0];
      lastEnding[1] = rot[j].R[1][0] * sin(theta+theta0) / comp[j].invR0 +  rot[j].R[1][1] * (1.0 - cos(theta+theta0)) / comp[j].invR0 + lastEnding[1];
      lastEnding[2] = rot[j].R[2][0] * sin(theta+theta0) / comp[j].invR0 +  rot[j].R[2][1] * (1.0 - cos(theta+theta0)) / comp[j].invR0 + lastEnding[2];
      printf("ending point of current arc: x %f y %f z %f\n",  lastEnding[0], lastEnding[1] ,  lastEnding[2]);

    }
    /* --------------------- If component is clothoids, update --------------------------------------------- ----------------*/
    else{

      fscanf(ifp,"%lf %lf %lf %lf %lf %lf",&comp[j].invR0, &comp[j].invRf, &comp[j].Ls, &rot[j].thetaX, &rot[j].thetaY, &rot[j].thetaZ);

      // update rotation matrix using Euler angle
      printf("... load arc of type : Clothoids\n");

      cX = cos(rot[j].thetaX);
      sX = sin(rot[j].thetaX);

      cY = cos(rot[j].thetaY);
      sY = sin(rot[j].thetaY);

      cZ = cos(rot[j].thetaZ);
      sZ = sin(rot[j].thetaZ);

      rot[j].R[0][0] = cX * cY;
      rot[j].R[1][0] = sX * cY;
      rot[j].R[2][0] =    - sY;

      rot[j].R[0][1] = cX * sY * sZ - sX * cZ;
      rot[j].R[1][1] = sX * sY * sZ + cX * cZ;
      rot[j].R[2][1] =      cY * sZ;

      rot[j].R[0][2] = cX * sY * cZ + sX *sZ;
      rot[j].R[1][2] = sX * sY * cZ - cX *sZ;
      rot[j].R[2][2] =      cY * cZ ;

      // update ending point by integrating numerically

      A = sqrt((comp[j].invRf - comp[j].invR0) / (2.0 * comp[j].Ls));

      aux0 = 0.0;
      aux1 = 0.0;

      dL = comp[j].Ls / 999.0;
      u[0] = 0.0;

      for (i = 1; i < 1000; i++){
	u[i] = u[i - 1] + dL;
      }

      for (i = 1; i < 1000; i++){

	La = u[i];
	Lb = u[i - 1];

	aux0 = (cos(pow((A * La),2.0) + comp[j].invR0 * La) + cos(pow((A * Lb),2.0) + comp[j].invR0 * Lb)) * dL / 2.0 + aux0;
	aux1 = (sin(pow((A * La),2.0) + comp[j].invR0 * La) + sin(pow((A * Lb),2.0) + comp[j].invR0 * Lb)) * dL / 2.0 + aux1;

      }

      lastEnding[0] = rot[j].R[0][0] * aux0 +  rot[j].R[0][1] * aux1 + lastEnding[0];
      lastEnding[1] = rot[j].R[1][0] * aux0 +  rot[j].R[1][1] * aux1 + lastEnding[1];
      lastEnding[2] = rot[j].R[2][0] * aux0 +  rot[j].R[2][1] * aux1 + lastEnding[2];
      printf("ending point of current arc: x %f y %f z %f\n",  lastEnding[0], lastEnding[1] ,  lastEnding[2]);
    }

    // print to console to check input parameters
    printf("%lf %lf %lf %lf %lf %lf\n", comp[j].invR0, comp[j].invRf, comp[j].Ls, lastEnding[0], lastEnding[1], lastEnding[2]);
    printf("%lf %lf %lf \n%lf %lf %lf \n%lf %lf %lf\n", rot[j].R[0][0], rot[j].R[0][1], rot[j].R[0][2], rot[j].R[1][0], rot[j].R[1][1], rot[j].R[1][2], rot[j].R[2][0], rot[j].R[2][1], rot[j].R[2][2]);

    j++;

  }

  fclose(ifp);

  return SM_OK;

}



//SM_STATUS convertMotionToCurve(std::vector<SM_OUTPUT> &motion, double tic,double nbIntervals,  std::vector<SM_CURVE_DATA>  &ApproxTraj) {
//  int i = 0, j = 0, k = 0;
//  int TrajType;
//  int zoneOut;
//  double TotalLength = 0.0;
//  double dcOut;
//  double ICloc[3];
//  SM_TIMES TimeSeg;
//  double t;
//
//  double Tloc,Jloc[3];
//  double aloc[3], vloc[3], xloc[3];
//  int timefile = 0;
//
//Tloc = 0;
//double tloctot;
//  for (i = 0; i < (3 * nbIntervals); i++){
//    tloctot += motion[i].Time[0];
//
//  }
//  cout << "tloc" << Tloc << endl;
//  timefile =0;
////   for (i = 0; i < (3 * (nbIntervals)); i++){
////     Tloc = motion[i].Time[0];
//// tloctot += Tloc;
////     for(t = 0;t<= Tloc; t+= tic) {
////
////       for (j = 0; j < 3; j++){
////
////
//// 	Jloc[j] = motion[i].Jerk[j];
////
//// 	ICloc[0] = motion[i].IC[j].a;
//// 	ICloc[1] = motion[i].IC[j].v;
//// 	ICloc[2] = motion[i].IC[j].x;
////
//// 	sm_AVX_TimeVar(ICloc, &Tloc, &Jloc[j], 1, &t, 1, &aloc[j], &vloc[j], &xloc[j]);
////       }
////
//// // cout << timefile << " " << endl;
//// //       fprintf(f,"%d %f %f %f %f %f %f\n", timefile,xloc[0], xloc[1], xloc[2], Jloc[0], Jloc[1], Jloc[2]);
////       timefile ++;
////     }
////   }
//
//double t0 = 0;
//double tl;
//int interval = 0;
//int index = 0;
//  for (t= 0.0; t< tloctot; t+= tic) {
//
//      if(t >= (t0 + motion[interval].Time[0])) {
//	t0  += motion[interval].Time[0];
//	interval ++;
//      }
//      for (j = 0; j < 3; j++){
//
//	    Jloc[j] = motion[interval].Jerk[j];
//	    ICloc[0] = motion[interval].IC[j].a;
//	    ICloc[1] = motion[interval].IC[j].v;
//	    ICloc[2] = motion[interval].IC[j].x;
//	    tl  = t-t0;
//	    sm_AVX_TimeVar(ICloc, &Tloc, &Jloc[j], 1, &tl, 1, &aloc[j], &vloc[j], &xloc[j]);
//      }
//// cout << "time " << t << endl;
//
//      ApproxTraj[index ].Pos[0]  = xloc[0];
//      ApproxTraj[index ].Pos[1]  = xloc[1];
//      ApproxTraj[index ].Pos[2]  = xloc[2];
//      ApproxTraj[index ].Vel[0]  = vloc[0];
//      ApproxTraj[index ].Vel[1]  = vloc[1];
//      ApproxTraj[index ].Vel[2]  = vloc[2];
//      ApproxTraj[index ].Acc[0]  = aloc[0];
//      ApproxTraj[index ].Acc[1]  = aloc[1];
//      ApproxTraj[index ].Acc[2]  = aloc[2];
//
////       if(t == 72.14) {
//// 	cout << "aque totototo" << endl;
////        cout << index << " " << ApproxTraj[index ].Pos[0] << " " << ApproxTraj[index ].Pos[1] << endl;
////       }
//      index ++;
//  }
//
//   for(j = index ; j< ApproxTraj.size(); j++) {
//      ApproxTraj[j ].Pos[0]  = xloc[0];
//      ApproxTraj[j ].Pos[1]  = xloc[1];
//      ApproxTraj[j ].Pos[2]  = xloc[2];
//      ApproxTraj[j ].Vel[0]  = vloc[0];
//      ApproxTraj[j ].Vel[1]  = vloc[1];
//      ApproxTraj[j ].Vel[2]  = vloc[2];
//      ApproxTraj[j ].Acc[0]  = aloc[0];
//      ApproxTraj[j ].Acc[1]  = aloc[1];
//      ApproxTraj[j ].Acc[2]  = aloc[2];
//
//  }
//
//  cout <<"tloctoto " << tloctot <<  " approxTrajSize " << ApproxTraj.size() << endl;
//}


SM_STATUS convertMotionToCurve_InAdvance(std::vector<SM_OUTPUT> &motion, double tic,double nbIntervals,  std::vector<SM_CURVE_DATA>  &ApproxTraj) {

  unsigned int i = 0, j = 0;
  std::vector<double> ICloc;
  double t = 0.0;
  ApproxTraj.clear();
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
    double t0 = 0.0;
  std::vector<double>  tl;
  int interval = 0;

  double tloctot = 0;
  int limit_Time = 0;
  SM_CURVE_DATA curveData;

  bzero(&curveData, sizeof(SM_CURVE_DATA));

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(3);
  vloc.resize(3);
  xloc.resize(3);
  tl.resize(1);
  Tloc.at(0) = 0;


  for (i = 0; i < motion.size(); i++){
    tloctot += motion[i].Time[0];
  }

limit_Time = round (tloctot/tic)+1;

for (int tt = 0; tt < limit_Time; tt++) {
      t = tt * tic;
      if((t > (t0 + motion[interval].Time[0]))&& (t <= tloctot )) {
        t0  += motion[interval].Time[0];
        interval ++;
      }

      for (j = 0; j < 3; j++){
        Jloc[0] = motion[interval].Jerk[j];
        ICloc[0] = motion[interval].IC[j].a;
        ICloc[1] = motion[interval].IC[j].v;
        ICloc[2] = motion[interval].IC[j].x;
        tl.at(0)  = t-t0;

        sm_AVX_TimeVar(ICloc, Tloc, Jloc, tl, aloc, vloc, xloc);

        curveData.Pos[j]  = xloc.at(0);
        curveData.Vel[j]  = vloc.at(0);
        curveData.Acc[j]  = aloc.at(0);
      }

      curveData.t = t;
      curveData.Jerk[0]  = motion[interval].Jerk[0];
      curveData.Jerk[1]  = motion[interval].Jerk[1];
      curveData.Jerk[2]  = motion[interval].Jerk[2];
      ApproxTraj.push_back(curveData);
  }

  return SM_OK;
}


SM_STATUS convertMotionToCurve_InAdvance(std::vector<SM_OUTPUT> &motion, double tic,double nbIntervals,  std::vector<SM_CURVE_DATA3>  &ApproxTraj) {

  unsigned int i = 0, j = 0;
  std::vector<double> ICloc;
  double t = 0.0;
  ApproxTraj.clear();
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
    double t0 = 0.0;
  std::vector<double>  tl;
  int interval = 0;

  double tloctot = 0;
  int limit_Time = 0;
  SM_CURVE_DATA3 curveData;

  bzero(&curveData, sizeof(SM_CURVE_DATA3));

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(3);
  vloc.resize(3);
  xloc.resize(3);
  tl.resize(1);
  Tloc.at(0) = 0;


  for (i = 0; i < motion.size(); i++){
    tloctot += motion[i].Time[0];
  }

limit_Time = round (tloctot/tic)+1;

for (int tt = 0; tt < limit_Time; tt++) {
      t = tt * tic;
      if((t > (t0 + motion[interval].Time[0]))&& (t <= tloctot )) {
        t0  += motion[interval].Time[0];
        interval ++;
      }

      for (j = 0; j < 3; j++){
        Jloc[0] = motion[interval].Jerk[j];
        ICloc[0] = motion[interval].IC[j].a;
        ICloc[1] = motion[interval].IC[j].v;
        ICloc[2] = motion[interval].IC[j].x;
        tl.at(0)  = t-t0;

        sm_AVX_TimeVar(ICloc, Tloc, Jloc, tl, aloc, vloc, xloc);

        curveData.Pos[j]  = xloc.at(0);
        curveData.Vel[j]  = vloc.at(0);
        curveData.Acc[j]  = aloc.at(0);
      }

      curveData.t = t;
      curveData.Jerk[0]  = motion[interval].Jerk[0];
      curveData.Jerk[1]  = motion[interval].Jerk[1];
      curveData.Jerk[2]  = motion[interval].Jerk[2];
      ApproxTraj.push_back(curveData);
  }

  return SM_OK;
}


SM_STATUS convertMotionToCurve(std::vector<SM_OUTPUT> &motion,  double tic,double nbIntervals,
                               std::vector<SM_CURVE_DATA>  &ApproxTraj) {
  unsigned int i = 0, j = 0;
  std::vector<double> ICloc;
  double t = 0.0;
  ApproxTraj.clear();
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
  int timefile = 0;
  double t0 = 0;
  std::vector<double>  tl;
  int interval = 0;

  double tloctot = 0;
  int limit_Time;
  SM_CURVE_DATA curveData;

  bzero(&curveData, sizeof(SM_CURVE_DATA));

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(1);
  vloc.resize(1);
  xloc.resize(1);
  tl.resize(1);
  Tloc.at(0) = 0;

  for (i = 0; i < motion.size(); i++){
    tloctot += motion[i].Time[0];
  }

  limit_Time = round (tloctot/tic)+1;
  timefile =0;
  for (int tt = 0; tt < limit_Time; tt++) {
      t = tt * tic;
      if((t > (t0 + motion[interval].Time[0])) && (t <= 3*motion[interval].Time[0] )) {
	    t0  += motion[interval].Time[0];
	    interval ++;
      }

      for (j = 0; j < 3; j++){
	    Jloc[0] = motion[interval].Jerk[j];
	    ICloc[0] = motion[interval].IC[j].a;
	    ICloc[1] = motion[interval].IC[j].v;
	    ICloc[2] = motion[interval].IC[j].x;
	    tl.at(0)  = t-t0;

	    sm_AVX_TimeVar(ICloc, Tloc, Jloc, tl, aloc, vloc, xloc);

	    curveData.Pos[j]  = xloc.at(0);
	    curveData.Vel[j]  = vloc.at(0);
	    curveData.Acc[j]  = aloc.at(0);
      }



      curveData.t = t;
      curveData.Jerk[0]  = motion[interval].Jerk[0];
      curveData.Jerk[1]  = motion[interval].Jerk[1];
      curveData.Jerk[2]  = motion[interval].Jerk[2];
      ApproxTraj.push_back(curveData);
  }

  return SM_OK;
}

SM_STATUS convertMotionToCurve3(std::vector<SM_OUTPUT> &motion, double tic,double nbIntervals,
                               std::vector<SM_CURVE_DATA3>  &ApproxTraj) {
  unsigned int i = 0, j = 0;
  std::vector<double> ICloc;
  double t = 0.0;
  ApproxTraj.clear();
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
  int timefile = 0;
  double t0 = 0;
  std::vector<double>  tl;
  int interval = 0;

  double tloctot = 0;
  int limit_Time;
  SM_CURVE_DATA3 curveData;

  bzero(&curveData, sizeof(SM_CURVE_DATA3));

  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(1);
  vloc.resize(1);
  xloc.resize(1);
  tl.resize(1);
  Tloc.at(0) = 0;

  for (i = 0; i < motion.size(); i++){
    tloctot += motion[i].Time[0];
  }

  limit_Time = round (tloctot/tic)+1;
  timefile =0;
  for (int tt = 0; tt < limit_Time; tt++) {
      t = tt * tic;
      if((t > (t0 + motion[interval].Time[0])) && (t <= 3*motion[interval].Time[0] )) {
	    t0  += motion[interval].Time[0];
	    interval ++;
      }

      for (j = 0; j < 3; j++){
	    Jloc[0] = motion[interval].Jerk[j];
	    ICloc[0] = motion[interval].IC[j].a;
	    ICloc[1] = motion[interval].IC[j].v;
	    ICloc[2] = motion[interval].IC[j].x;
	    tl.at(0)  = t-t0;

	    sm_AVX_TimeVar(ICloc, Tloc, Jloc, tl, aloc, vloc, xloc);

	    curveData.Pos[j]  = xloc.at(0);
	    curveData.Vel[j]  = vloc.at(0);
	    curveData.Acc[j]  = aloc.at(0);
      }



      curveData.t = t;
      curveData.Jerk[0]  = motion[interval].Jerk[0];
      curveData.Jerk[1]  = motion[interval].Jerk[1];
      curveData.Jerk[2]  = motion[interval].Jerk[2];
      ApproxTraj.push_back(curveData);
  }
  
  return SM_OK;
}


SM_STATUS convertMotionToCurve2(std::vector<SM_OUTPUT> &motion, int nbAxis, double tic,double nbIntervals,
                               std::vector<SM_CURVE_DATA>  &ApproxTraj) {
  unsigned int i = 0;
  int j = 0;
  std::vector<double> ICloc;
  double t = 0.0;
  ApproxTraj.clear();
  std::vector<double> Tloc,Jloc;
  std::vector<double> aloc, vloc, xloc;
  int timefile = 0;
  double t0 = 0;
  std::vector<double>  tl;
  int interval = 0;

  double tloctot = 0;
  int limit_Time;
  SM_CURVE_DATA curveData;

  bzero(&curveData, sizeof(SM_CURVE_DATA));

  curveData.Pos.resize(nbAxis);
  curveData.Vel.resize(nbAxis);
  curveData.Acc.resize(nbAxis);
 curveData.Jerk.resize(nbAxis);
  ICloc.resize(3);
  Tloc.resize(1);
  Jloc.resize(1);
  aloc.resize(1);
  vloc.resize(1);
  xloc.resize(1);
  tl.resize(1);
  Tloc.at(0) = 0;

  for (i = 0; i < motion.size(); i++){
    tloctot += motion[i].Time[0];
  }

  limit_Time = round (tloctot/tic)+1;
  timefile =0;
  for (int tt = 0; tt < limit_Time; tt++) {
      t = tt * tic;
      if((t > (t0 + motion[interval].Time[0])) && (t <= 3*motion[interval].Time[0] )) {
	    t0  += motion[interval].Time[0];
	    interval ++;
      }

      for (j = 0; j < nbAxis; j++){
	    Jloc[0] = motion[interval].Jerk[j];
	    ICloc[0] = motion[interval].IC[j].a;
	    ICloc[1] = motion[interval].IC[j].v;
	    ICloc[2] = motion[interval].IC[j].x;
	    tl.at(0)  = t-t0;

	    sm_AVX_TimeVar(ICloc, Tloc, Jloc, tl, aloc, vloc, xloc);

	    curveData.Pos[j]  = xloc.at(0);
	    curveData.Vel[j]  = vloc.at(0);
	    curveData.Acc[j]  = aloc.at(0);
      }



      curveData.t = t;
      for(j=0; j<nbAxis; j++){
      curveData.Jerk[j]  = motion[interval].Jerk[j];
      //curveData.Jerk[1]  = motion[interval].Jerk[1];
      //curveData.Jerk[2]  = motion[interval].Jerk[2];
      }
      ApproxTraj.push_back(curveData);
  }

  return SM_OK;
}




SM_STATUS sm_Main(std::vector<SM_OUTPUT> &motion, int*nbJerkConst , std::string fileName){
  /* Main function to take input file and compute the output jerk evolution
     -- motion[] : array of output
     -- nbJerkConst : number of output segment
  */

  double width, height;
  double tic = 0.001;
  int nbIntervals;
  double tu, ts;
  SM_LIMITS Lim;
  std::vector<SM_CURVE_DATA>  IdealTraj;
  std::vector<SM_CURVE_DATA>  ApproxTraj;
  std::vector<SM_COND_DIM> IC;
  std::vector<SM_COND_DIM> FC;
  std::vector<double> Timp;
  std::vector<int> IntervIndex;
  std::list<Path> path;
  std::string fileNameApprox;
   std::string fileNameIdeal;

  ChronoOn();

  fileNameApprox.clear();
  fileNameApprox.append("approxTraj.dat");
  fileNameIdeal.clear();
  fileNameIdeal.append("idealTraj.dat");

  // Custom Params
  Lim.maxJerk = 0.9;
  Lim.maxAcc  = 0.6;
  Lim.maxVel  = 0.15;
  nbIntervals = 40; // on definit le nombre d'interval

  if (parseSvg(fileName, path, &width, &height) == SM_ERROR) {
    SM_LOG(SM_LOG_ERROR, " parse ERROR");
    return SM_ERROR;
  }
  SM_LOG(SM_LOG_INFO, " parse is OK");
  constructTrajSvg(path,  tic, Lim, IdealTraj);
   plotIdealTraj(fileNameIdeal, IdealTraj, width, height) ;

// on met les longueur de IC,FC et Timp a nbIntervals, mais il faut trouver un algorithme pour calculer cet Interval
 IC.resize(nbIntervals);
 FC.resize(nbIntervals);
 Timp.resize(nbIntervals);
 IntervIndex.resize(nbIntervals + 1);


//   if (sm_ComputeCondition(IdealTraj, IC, FC, Timp, IntervIndex) != 0){
//     printf("Compute Problem \n");
//     return SM_ERROR;
//   }

// on distribute l'espace dans motion et ApproxTraj, autant de points que IdealTraj
  motion.resize(IdealTraj.size());
  ApproxTraj.resize(IdealTraj.size());

// maintenant, il y a 40 donnee dans Timp
  if (sm_SolveWithoutOpt(IC, FC, Timp, motion) != 0){
    SM_LOG(SM_LOG_INFO, "Solve Problem");
    return SM_ERROR;
  }

  convertMotionToCurve(motion, tic, nbIntervals, ApproxTraj);
//   plotIdealTraj(fileNameApprox , ApproxTraj, width, height) ;
plotApproxTraj(fileNameApprox, ApproxTraj, width, height) ;
  ChronoPrint("");
  ChronoTimes(&tu, &ts);
  ChronoOff();
  *nbJerkConst = nbIntervals *3;
  return SM_OK;
}




//SM_STATUS sm_Main(std::vector<SM_OUTPUT> &motion, int*nbJerkConst , std::string fileName){
//  /* Main function to take input file and compute the output jerk evolution
//     -- motion[] : array of output
//     -- nbJerkConst : number of output segment
//  */
//
//  double width, height;
//  double tic = 0.001;
//  int m, nbIntervals;
//  double tu, ts, PosInit[3];
//  SM_LIMITS Lim;
//  std::vector<SM_CURVE_DATA>  IdealTraj;
//  std::vector<SM_CURVE_DATA>  ApproxTraj;
//  std::vector<SM_COND_DIM> IC;
//  std::vector<SM_COND_DIM> FC;
//  std::vector<double> Timp;
//  std::vector<int> IntervIndex;
//  std::list<Path> path;
//  std::string fileNameApprox;
//   std::string fileNameIdeal;
//
//  ChronoOn();
//
//  fileNameApprox.clear();
//  fileNameApprox.append("approxTraj.dat");
//  fileNameIdeal.clear();
//  fileNameIdeal.append("idealTraj.dat");
//
//  // Custom Params
//  Lim.maxJerk = 0.9;
//  Lim.maxAcc  = 0.6;
//  Lim.maxVel  = 0.15;
//  nbIntervals = 40;
//
//  if (parseSvg(fileName, path, &width, &height) == SM_ERROR) {
//    cout << " parse ERROR" << endl;
//    return SM_ERROR;
//  }
//  cout << " parse is OK" << endl;
//  constructTrajSvg(path,  tic, Lim, IdealTraj);
//   plotIdealTraj(fileNameIdeal, IdealTraj, width, height) ;
//
// IC.resize(nbIntervals);
// FC.resize(nbIntervals);
// Timp.resize(nbIntervals);
// IntervIndex.resize(nbIntervals + 1);
//
//
//  if (sm_ComputeCondition(IdealTraj, IC, FC, Timp, IntervIndex) != 0){
//    printf("Compute Problem \n");
//    return SM_ERROR;
//  }
//  motion.resize(IdealTraj.size());
//  ApproxTraj.resize(IdealTraj.size());
//
//  if (sm_SolveWithoutOpt(IC, FC, Timp, motion) != 0){
//    printf("Solve Problem \n");
//    return SM_ERROR;
//  }
//
//  convertMotionToCurve(motion, tic, nbIntervals, ApproxTraj);
////   plotIdealTraj(fileNameApprox , ApproxTraj, width, height) ;
//plotApproxTraj(fileNameApprox, ApproxTraj, width, height) ;
//  ChronoPrint("");
//  ChronoTimes(&tu, &ts);
//  ChronoOff();
//  *nbJerkConst = nbIntervals *3;
//  return SM_OK;
//}



SM_STATUS parsePath(std::istringstream &iss, std::list<Path> &path, double svg_ratio, double svg_y_sign, double svg_y_offset, double scalex, double scaley) {
  Path lpath;
  SubPath lsubpath;
  std::string element;
  std::stringstream elementStream;
  std::string dummy;
  //  long double ldValue1, ldValue2;
  std::string key (",");
  size_t found;
  bool result, relative;

  result= static_cast<bool>(iss >> element );

  while( !result || !iss.eof() )
    {// std::cout << element << std::endl;
      //
      elementStream.clear();
      if(element=="M" || element=="m")
	{
	  if(0) {
	    SM_LOG(SM_LOG_DEBUG, "=======================");
	    SM_LOG(SM_LOG_DEBUG, "   New Path");
	  }
	  if(element=="M") {relative= false;}
	  else {relative= true;}
	  iss >> element ;
	  found=element.rfind(key);
	  if (found!=std::string::npos)
	  element.replace (found,key.length()," ");
	  elementStream << element;
	  elementStream >> lpath.origin.x >> lpath.origin.y;
	  lpath.origin.x = lpath.origin.x* svg_ratio * scalex;
	  lpath.origin.y = (lpath.origin.y* svg_ratio * svg_y_sign + svg_y_offset)* scaley;
	  lpath.nbSubPath = 0;
	  path.push_back(lpath);
      SM_LOG(SM_LOG_DEBUG, "path origin : x = "<< path.back().origin.x << " y = " << path.back().origin.y);
      cout << path.back().origin.x << " " << path.back().origin.y << endl;
	}
      else if(element=="L") {
	iss >> element ;
	found=element.rfind(key);
	if (found!=std::string::npos)
	  element.replace (found,key.length()," ");
	elementStream << element;

	elementStream >> lsubpath.end.x >> lsubpath.end.y;
	lsubpath.end.x =  lsubpath.end.x* svg_ratio* scalex - lpath.origin.x;
	lsubpath.end.y = (lsubpath.end.y* svg_ratio  * svg_y_sign + svg_y_offset)* scaley - lpath.origin.y;

	if(path.back().nbSubPath == 0) {
	  lsubpath.start.x = path.back().origin.x- lpath.origin.x;
	  lsubpath.start.y = path.back().origin.y - lpath.origin.y;
	} else {
	  lsubpath.start.x = path.back().subpath.back().end.x;
	  lsubpath.start.y = path.back().subpath.back().end.y;
	}
	lsubpath.type = LINE;
	path.back().subpath.push_back(lsubpath);
	path.back().nbSubPath ++;
	SM_LOG(SM_LOG_DEBUG,  "LINE");
	SM_LOG(SM_LOG_DEBUG, "start point " << path.back().subpath.back().start.x << " " << path.back().subpath.back().start.y);
	SM_LOG(SM_LOG_DEBUG, "end   point " << path.back().subpath.back().end.x << " " << path.back().subpath.back().end.y);
      }

    else if(element=="C") {
	iss >> element;
	sscanf(element.c_str(), "%lf,%lf", &lsubpath.bezier3[0].x, &lsubpath.bezier3[0].y);
	lsubpath.bezier3[0].x = lsubpath.bezier3[0].x* svg_ratio* scalex - lpath.origin.x;
	lsubpath.bezier3[0].y =(lsubpath.bezier3[0].y* svg_ratio * svg_y_sign+ svg_y_offset)* scaley - lpath.origin.y;
	iss >> element;
	sscanf(element.c_str(), "%lf,%lf", &lsubpath.bezier3[1].x, &lsubpath.bezier3[1].y);
	lsubpath.bezier3[1].x = lsubpath.bezier3[1].x* svg_ratio* scalex - lpath.origin.x;
	lsubpath.bezier3[1].y = (lsubpath.bezier3[1].y* svg_ratio * svg_y_sign+ svg_y_offset)* scaley - lpath.origin.y;
	iss >> element;
	sscanf(element.c_str(), "%lf,%lf", &lsubpath.end.x, &lsubpath.end.y);
	lsubpath.end.x = lsubpath.end.x* svg_ratio* scalex - lpath.origin.x;
	lsubpath.end.y = (lsubpath.end.y* svg_ratio * svg_y_sign+ svg_y_offset)* scaley - lpath.origin.y;
	lsubpath.type = BEZIER3;
	if(path.back().nbSubPath == 0) {
	  lsubpath.start.x = path.back().origin.x- lpath.origin.x ;
	  lsubpath.start.y = path.back().origin.y- lpath.origin.y;
	} else {
	  lsubpath.start.x = path.back().subpath.back().end.x;
	  lsubpath.start.y = path.back().subpath.back().end.y;
	}
	path.back().subpath.push_back(lsubpath);
	path.back().nbSubPath ++;
	if(0) {
	  std::cout << "BEZIER3" << std::endl;
	  std::cout << "start  point " << path.back().subpath.back().start.x << " " << path.back().subpath.back().start.y << std::endl;
	  std::cout << "end    point " << path.back().subpath.back().end.x << " " << path.back().subpath.back().end.y << std::endl;
	  std::cout << "cntrl1 point " << path.back().subpath.back().bezier3[0].x << " " << path.back().subpath.back().bezier3[0].y << std::endl;
	  std::cout << "cntrl2 point " << path.back().subpath.back().bezier3[1].x << " " << path.back().subpath.back().bezier3[1].y << std::endl;
	}
      }
    else if(element=="c" || element=="l") {
        std::cerr << "Error! Your SVG path contains relative coordinates, \
            which are not understood by softMotion." << endl << 
            "Please disable relative coordinate in the editor settings." << endl;
        return SM_ERROR;
    }
      iss >> element;

    }
  return SM_OK;
}

SM_STATUS parseSvg(std::string fileName, std::list<Path> &path, double* width, double* height) {
  std::string element;

  xmlDocPtr doc;
  xmlNodePtr root, cur, cur2, cur3, cur4;
  double ldv1, ldv2, ldv3, ldv4, ldv5, ldv6;
  xmlChar *attribute;
  std::istringstream iss;
  xmlLineNumbersDefault(1);
  doc= xmlParseFile(fileName.c_str());
  double svg_ratio = (1.0 / 3.5433)/1000.0;
  double svg_y_sign = -1.0;
  double svg_y_offset;
  SM_LOG(SM_LOG_INFO, "parsing " << fileName);
  if(doc==NULL) {
      SM_LOG(SM_LOG_ERROR, "parseSvg(): document \"" << fileName << "\" does not exist or was not parsed successfully by libxml2.");
      return SM_ERROR;
  }
  root = xmlDocGetRootElement(doc);

  if(root==NULL) {
      SM_LOG(SM_LOG_ERROR, "parseSvg(): document \""<< fileName << "\" is empty.");
      xmlFreeDoc(doc);
      return SM_ERROR;
  }

  attribute = xmlGetProp(root, (xmlChar*)"width");
  iss.str((char*)attribute);
  iss >> *width;
  *width = *width *svg_ratio;
  iss.clear();
  xmlFree(attribute);
  attribute = xmlGetProp(root, (xmlChar*)"height");
  iss.str((char*)attribute);
  iss >> *height;
  *height =  *height *svg_ratio * svg_y_sign;
  iss.clear();
  xmlFree(attribute);
  SM_LOG(SM_LOG_DEBUG, " width " << *width << " height " << *height);

  svg_y_offset =  svg_y_sign* *height;

  for(cur= root->xmlChildrenNode; cur!=NULL; cur= cur->next)
    {

      ldv1 = 1.0;
	ldv4 = 1.0;
      if(!xmlStrcmp(cur->name, (const xmlChar *)"g")) {
	ldv1 = 1.0;
	ldv4 = 1.0;
        attribute = xmlGetProp(cur, (xmlChar*)"transform");
	if(attribute != NULL) {
	    iss.str((char*)attribute);
	    iss >> element;
	    sscanf(element.c_str(), "matrix(%lf,%lf,%lf,%lf,%lf,%lf)", &ldv1, &ldv2, &ldv3, &ldv4, &ldv5, &ldv6);
	    iss.clear();
        SM_LOG(SM_LOG_DEBUG,  "scale " << ldv1 << " " << ldv4);
	    xmlFree(attribute);
	}

	for(cur2= cur->xmlChildrenNode; cur2!=NULL; cur2= cur2->next) {
	  if(!xmlStrcmp(cur2->name, (const xmlChar *)"path")) {

	if(0) {
		std::cout << "path foud in a group " << cur2->name << std::endl;
	}
	    attribute = xmlGetProp(cur2, (xmlChar*)"d");
	    iss.str((char*)attribute);
	    parsePath(iss, path, svg_ratio, svg_y_sign, svg_y_offset, ldv1,ldv4);
	if(0) {

	     cout << "scale " << ldv1 << " " << ldv4 << endl;
	}
	    xmlFree(attribute);
	  }

	  if(!xmlStrcmp(cur2->name, (const xmlChar *)"g")) {
	    	ldv1 = 1.0;
		ldv4 = 1.0;
		attribute = xmlGetProp(cur2, (xmlChar*)"transform");
		if(attribute != NULL) {
		    iss.str((char*)attribute);
		    iss >> element;
		    SM_LOG(SM_LOG_INFO, attribute << " " << element);
		    sscanf(element.c_str(), "matrix(%lf,%lf,%lf,%lf,%lf,%lf)", &ldv1, &ldv2, &ldv3, &ldv4, &ldv5, &ldv6);
            SM_LOG(SM_LOG_DEBUG,  "scale cur2 " << ldv1 << " " << ldv4);
		     iss.clear();
		      xmlFree(attribute);
		}

	    for(cur3= cur2->xmlChildrenNode; cur3!=NULL; cur3= cur3->next) {

	      if(!xmlStrcmp(cur3->name, (const xmlChar *)"g")) {

		  ldv1 = 1.0;
		  ldv4 = 1.0;
		  attribute = xmlGetProp(cur3, (xmlChar*)"transform");
		  if(attribute != NULL) {
		      iss.str((char*)attribute);
		      iss >> element;
		      sscanf(element.c_str(), "matrix(%lf,%lf,%lf,%lf,%lf,%lf)", &ldv1, &ldv2, &ldv3, &ldv4, &ldv5, &ldv6);
              SM_LOG(SM_LOG_DEBUG,  "scale " << ldv1 << " " << ldv4);
		       iss.clear();
			xmlFree(attribute);
		  }


		   for(cur4= cur3->xmlChildrenNode; cur4!=NULL; cur4= cur4->next) {

		     if(!xmlStrcmp(cur4->name, (const xmlChar *)"path")) {
		    SM_LOG(SM_LOG_DEBUG, "path foud in a group " << cur4->name);
		    attribute = xmlGetProp(cur4, (xmlChar*)"d");
		    iss.str((char*)attribute);
		    parsePath(iss, path, svg_ratio, svg_y_sign, svg_y_offset, ldv1,ldv4);
		     SM_LOG(SM_LOG_DEBUG, "scale " << ldv1 << " " << ldv4);
		    xmlFree(attribute);
		  }
		}

	      }
	      if(!xmlStrcmp(cur3->name, (const xmlChar *)"path")) {
		SM_LOG(SM_LOG_DEBUG,  "path foud in a sub group " << cur3->name);
		attribute = xmlGetProp(cur3, (xmlChar*)"d");
		iss.str((char*)attribute);
		SM_LOG(SM_LOG_DEBUG, "path cur3");
		parsePath(iss, path, svg_ratio, svg_y_sign, svg_y_offset, ldv1,ldv4);
		SM_LOG(SM_LOG_DEBUG, "path out");
		xmlFree(attribute);
	      }
	    }
	  }
	}
      }

      if(!xmlStrcmp(cur->name, (const xmlChar *)"path")) {
	attribute = xmlGetProp(cur, (xmlChar*)"d");
	iss.str((char*)attribute);
	parsePath(iss, path, svg_ratio, svg_y_sign, svg_y_offset, ldv1,ldv4);
	xmlFree(attribute);
      }
      else {

      }
    }
  SM_LOG(SM_LOG_DEBUG,  "Number of path : " << path.size());
  if (path.size()==0) {
    SM_LOG(SM_LOG_ERROR,  "There is no path in your file");
    return SM_ERROR;
  }
  return SM_OK;
}




SM_STATUS saveTraj(std::string fileName, std::vector<SM_CURVE_DATA> &traj)
{
  int i =0;
  FILE * f = NULL;
  //printf("traj.size() %d\n", (int)traj.size() );
  f = fopen(fileName.c_str(),"w");
  if(f == NULL) {
    SM_LOG(SM_LOG_ERROR, "saveTraj : cannot open file");
    return SM_ERROR;
  }

  for(i=0; i<(int)traj.size(); i++){
    fprintf(f,"%d %f %f %f %f %f\n", i,traj[i].Pos[0],traj[i].Pos[1],traj[i].Pos[2], traj[i].Vel[0], traj[i].Acc[0] );
  }
  fclose(f);



  //for(i=0; i<(int)traj.size() -1; i++){
    std::string str;
    str.clear();
    str += "FULL_";
    str += fileName.c_str();
    
 f = fopen(str.c_str(),"w");
  if(f == NULL) {
    SM_LOG(SM_LOG_ERROR, "saveTraj : cannot open file");
    return SM_ERROR;
  }
  
  for(i=0; i<(int)traj.size(); i++){
  
    fprintf(f,"%d",i);
 
    for(unsigned u=0; u<traj[i].Pos.size(); u++) {

      fprintf(f," %f", traj[i].Pos[u]);
    }
    fprintf(f,"\n");

  }

  fclose(f);
  return SM_OK;
}


SM_STATUS saveTraj(std::string fileName, std::vector<SM_CURVE_DATA3> &traj)
{
  int i =0;
  FILE * f = NULL;

  f = fopen(fileName.c_str(),"w");
  if(f == NULL) {
    SM_LOG(SM_LOG_ERROR, "saveTraj : cannot open file");
    return SM_ERROR;
  }

  for(i=0; i<(int)traj.size() -1; i++){
    fprintf(f,"%d %f %f %f %f %f\n", i,traj[i].Pos[0],traj[i].Pos[1],traj[i].Pos[2], traj[i].Vel[0], traj[i].Acc[0] );
  }
  fclose(f);
  return SM_OK;
}



SM_STATUS plotIdealTraj(std::string fileName, std::vector<SM_CURVE_DATA> &IdealTraj, double width, double height)
{
  int i =0;
  FILE * f = NULL;

  f = fopen(fileName.c_str(),"w");

  if(f == NULL) {
    SM_LOG(SM_LOG_ERROR, "plotIdealTraj : cannot open file");
    return SM_ERROR;
  }


  for(i=0; i<(int)IdealTraj.size() -1; i++){
    fprintf(f,"%d %f %f %f %f %f\n", i,IdealTraj[i].Pos[0],IdealTraj[i].Pos[1],IdealTraj[i].Pos[2], IdealTraj[i].Vel[0], IdealTraj[i].Acc[0] );
  }
  fclose(f);

  //
  // Using the GnuplotException class
  //
  try
    {
      Gnuplot g1("lines");
      g1.reset_plot();
      g1.cmd((char*)"set term wxt");
//    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);
      g1.cmd("set size ratio -1");
      g1.set_xautoscale();
      g1.set_yautoscale();
      g1.set_grid();
      g1.plotfile_xy(fileName.c_str(), 2, 3);
      wait_for_key();

    }
    catch (GnuplotException ge)
    {
      SM_LOG(SM_LOG_ERROR, ge.what());
    }
  return SM_OK;
}


SM_STATUS plotIdealTraj(std::string fileName, std::vector<SM_CURVE_DATA3> &IdealTraj, double width, double height)
{
  int i =0;
  FILE * f = NULL;

  f = fopen(fileName.c_str(),"w");

  if(f == NULL) {
    SM_LOG(SM_LOG_ERROR, "plotIdealTraj : cannot open file");
    return SM_ERROR;
  }


  for(i=0; i<(int)IdealTraj.size() -1; i++){
    fprintf(f,"%d %f %f %f %f %f\n", i,IdealTraj[i].Pos[0],IdealTraj[i].Pos[1],IdealTraj[i].Pos[2], IdealTraj[i].Vel[0], IdealTraj[i].Acc[0] );
  }
  fclose(f);

  //
  // Using the GnuplotException class
  //
  try
    {
      Gnuplot g1("lines");
      g1.reset_plot();
      g1.cmd((char*)"set term wxt");
//    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);
      g1.cmd("set size ratio -1");
      g1.set_xautoscale();
      g1.set_yautoscale();
      g1.set_grid();
      g1.plotfile_xy(fileName.c_str(), 2, 3);
      wait_for_key();

    }
    catch (GnuplotException ge)
    {
      SM_LOG(SM_LOG_ERROR, ge.what());
    }
  return SM_OK;
}

SM_STATUS plotApproxTraj(std::string fileName, std::vector<SM_CURVE_DATA> &IdealTraj, double width, double height)
{
  int i =0;
  FILE * f = NULL, * f2 = NULL, * f3 = NULL;
double  fx, fy, fz, fvx, fax;
int fi;
  f = fopen(fileName.c_str(),"w");
  f2 = fopen("idealTraj.dat","r");
  f3 = fopen("error.dat","w");

  for(i=0; i<(int)IdealTraj.size() -1; i++){
    fprintf(f,"%d %lf %lf %lf %lf %lf\n", i,IdealTraj[i].Pos[0],IdealTraj[i].Pos[1],IdealTraj[i].Pos[2], IdealTraj[i].Vel[0], IdealTraj[i].Acc[0] );
   fscanf(f2,"%d %lf %lf %lf %lf %lf\n", &fi, &fx, &fy, &fz, &fvx, &fax);
   cout  << fi << " " << fx << endl;
 fprintf(f3,"%d %lf\n",fi, sqrt((IdealTraj[i].Pos[0]-fx)*(IdealTraj[i].Pos[0]-fx)
 + (IdealTraj[i].Pos[1]-fy)*(IdealTraj[i].Pos[1]-fy)+ (IdealTraj[i].Pos[2]-fz)*(IdealTraj[i].Pos[2]-fz)));
  }
  fclose(f);
  fclose(f2);
fclose(f3);
  //
  // Using the GnuplotException class
  //
  try
    {
      Gnuplot g1("lines");
      g1.reset_plot();
      g1.cmd((char*)"set term wxt");
//    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);
      g1.cmd("set size ratio -1");
      g1.set_xautoscale();
      g1.set_yautoscale();
      g1.set_grid();
      g1.plotfile_xy(fileName.c_str(), 2, 3);

      g1.plotfile_xy("idealTraj.dat", 2,3);



//         Gnuplot g2("lines");
//       g2.reset_plot();
//       g2.cmd((char*)"set term wxt");
//*/    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);*/
//       g2.cmd("set size ratio -1");
//       g2.set_xautoscale();
//       g2.set_yautoscale();
//       g2.set_grid();
//       g2.plotfile_xy("error.dat", 1, 2);
     wait_for_key();
    }
    catch (GnuplotException ge)
    {
      cout << ge.what() << endl;
    }
  return SM_OK;
}


SM_STATUS plotApproxTraj(std::string fileName, std::vector<SM_CURVE_DATA3> &IdealTraj, double width, double height)
{
  int i =0;
  FILE * f = NULL, * f2 = NULL, * f3 = NULL;
double  fx, fy, fz, fvx, fax;
int fi;
  f = fopen(fileName.c_str(),"w");
  f2 = fopen("idealTraj.dat","r");
  f3 = fopen("error.dat","w");

  for(i=0; i<(int)IdealTraj.size() -1; i++){
    fprintf(f,"%d %lf %lf %lf %lf %lf\n", i,IdealTraj[i].Pos[0],IdealTraj[i].Pos[1],IdealTraj[i].Pos[2], IdealTraj[i].Vel[0], IdealTraj[i].Acc[0] );
   fscanf(f2,"%d %lf %lf %lf %lf %lf\n", &fi, &fx, &fy, &fz, &fvx, &fax);
   cout  << fi << " " << fx << endl;
 fprintf(f3,"%d %lf\n",fi, sqrt((IdealTraj[i].Pos[0]-fx)*(IdealTraj[i].Pos[0]-fx)
 + (IdealTraj[i].Pos[1]-fy)*(IdealTraj[i].Pos[1]-fy)+ (IdealTraj[i].Pos[2]-fz)*(IdealTraj[i].Pos[2]-fz)));
  }
  fclose(f);
  fclose(f2);
fclose(f3);
  //
  // Using the GnuplotException class
  //
  try
    {
      Gnuplot g1("lines");
      g1.reset_plot();
      g1.cmd((char*)"set term wxt");
//    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);
      g1.cmd("set size ratio -1");
      g1.set_xautoscale();
      g1.set_yautoscale();
      g1.set_grid();
      g1.plotfile_xy(fileName.c_str(), 2, 3);

      g1.plotfile_xy("idealTraj.dat", 2,3);



//         Gnuplot g2("lines");
//       g2.reset_plot();
//       g2.cmd((char*)"set term wxt");
//*/    g1.set_xrange(0, width);
//    g1.set_yrange(0, -1*height);*/
//       g2.cmd("set size ratio -1");
//       g2.set_xautoscale();
//       g2.set_yautoscale();
//       g2.set_grid();
//       g2.plotfile_xy("error.dat", 1, 2);
     wait_for_key();
    }
    catch (GnuplotException ge)
    {
      cout << ge.what() << endl;
    }
  return SM_OK;
}


SM_STATUS parsePath_sinus(std::list<Path> &path, double tic) {
    Path lpath;
    SubPath lsubpath;
    lsubpath.sinus.start.x = 0.0;
    lsubpath.sinus.start.y = 0.0;
    lsubpath.sinus.phase = 0.0;
    lsubpath.sinus.amplitude = 0.05;
    lsubpath.sinus.frequency = 10.0;
    lsubpath.type = SINUS;
    lpath.subpath.push_back(lsubpath);
    path.push_back(lpath);

    return SM_OK;
}

/*
  Calculate parametric value of x or y given t and the four point
  coordinates of a cubic bezier curve.
*/
Point2D bezier_point(double t, Point2D start, Point2D control_1, Point2D control_2, Point2D end) {
  Point2D out;

  out.x =  double(
             start.x * (1.0 - t) * (1.0 - t)  * (1.0 - t)
    + 3.0 *  control_1.x * (1.0 - t) * (1.0 - t)  * t
    + 3.0 *  control_2.x * (1.0 - t) * t          * t
    +              end.x * t         * t          * t   );
  out.y =  double(
             start.y * (1.0 - t) * (1.0 - t)  * (1.0 - t)
    + 3.0 *  control_1.y * (1.0 - t) * (1.0 - t)  * t
    + 3.0 *  control_2.y * (1.0 - t) * t          * t
    +              end.y * t         * t          * t   );

  return out;
}

/*
  Approximate length of the Bezier curve which starts at "start" and
  is defined by "c". According to Computing the Arc Length of Cubic Bezier Curves
  there is no closed form integral for it.
*/

double bezier_length (Point2D start, Point2D control_1, Point2D control_2, Point2D end, double step)
{
  double t;
  Point2D dot;
  Point2D previous_dot;
  double length = 0.0;
  for (t = 0; t <= 1.0; t = t+step) {
    dot = bezier_point (t, start, control_1, control_2, end);
    if (t > 0) {
      double x_diff = dot.x - previous_dot.x;
      double y_diff = dot.y - previous_dot.y;
      length += sqrt (x_diff * x_diff + y_diff * y_diff);
    }
    previous_dot = dot;
  }
  return length;
}

double length_cercle(std::list<Path> &path){
  double rad = 0.0;
  double length = 0.0;
  rad = path.back().subpath.front().cercle.radius;
  length = 2 * PI * rad;

  return length;
}

double length_parabol(std::list<Path> &path){
  double length;
  double start_x = path.back().subpath.front().parabol.start_x;
  double end_x = path.back().subpath.front().parabol.end_x;
  double a = path.back().subpath.front().parabol.a;
  length = end_x*sqrt(end_x*end_x + a*a) + a*a*log(end_x+sqrt(end_x*end_x + a*a)) -
           start_x*sqrt(start_x*start_x + a*a) - a*a*log(start_x+sqrt(start_x*start_x + a*a));

  return length;
}

double length_sinus(std::list<Path> &path, std::vector<IndiceTrace> &indice_trace){
  double amp = path.back().subpath.front().sinus.amplitude;
  double freq = path.back().subpath.front().sinus.frequency;
  double phas = path.back().subpath.front().sinus.phase;
  double length = 0.0;
  double length_dis = 0.0;
  double t = 0.0;
  double tic = 0.0001;
  int size;
  double x_post = 0.0;
  double y_post = 0.0;
  double x_pre = 0.0;
  double y_pre = 0.0;
  IndiceTrace ind_tr;
  size = (int) (path.back().subpath.front().sinus.length_x / tic);
  for (int i = 0; i< size; i++ ){
      x_post = t;
      y_post = amp * sin(2 * PI * freq * x_post + phas);
      length_dis = sqrt((x_post - x_pre)*(x_post - x_pre)+(y_post - y_pre)*(y_post - y_pre));
      length += length_dis;
      ind_tr.x = x_post;
      ind_tr.y = y_post;
      ind_tr.length_accumul = length;
      x_pre = x_post;
      y_pre = y_post;

      indice_trace.push_back(ind_tr);
      t += tic;
  }
  return length;
}


SM_STATUS constructTrajSvg(std::list<Path> &path, double tic, SM_LIMITS Lim, std::vector<SM_CURVE_DATA> &IdealTraj)
{

  //SM_LINE_ARC PathComp[], SM_ROT Rot[], int nbComp, double PosInit[3], SM_LIMITS Lim, double tic, int *nbPoints, SM_CURVE_DATA IdealTraj[SM_NB_DISC_MAX]) {

//  This function take a serie of lines and arcs (using Euler spiral parameter)  and convert into a discretized trajectory
//      -------- PathComp[] : Line & Arc definition
//      -------- Rot[]      : Rotation Information (Rotation matrix)
//      -------- nbComp     : total number of line or arc
//      -------- PosInit    : Stating Position of the path
//      -------- tic        : Time step for discretization (in second)
//      -------- nbPoints   : Output number of sample points (echantillonage)
//      -------- IdealTraj[]: Discretized output information of the input curve


  std::list<SubPath>::iterator iter;
  double lllength = 0.0;
  int nbPoints = 0;
  unsigned int i = 0;
  unsigned int j = 0;
  int TrajType = 0;
  int zoneOut = 0;
  double TotalLength = 0.0;
  double dcOut = 0.0;
  SM_COND ICloc, FCloc;
  SM_TIMES TimeSeg;

  std::vector<double> Time;
  Time.resize(7);
  std::vector<double> IC;
  IC.resize(3);
  std::vector<double> J;
  J.resize(7);
  std::vector<double> t, ddu, du, u;
//    double *t = NULL, *ddu =  NULL, *du = NULL, *u = NULL;
  double total_time = 0.0;
   std::vector<double> Lac ;
  Point2D lpoint;
  std::vector<double> dis_pos;
  std::vector<double> distance_tal;
  std::vector<IndiceTrace> indice_trace;

  double pos_x = 0.0;
  double pos_y = 0.0;
  double pos_z = 0.0;
  double para = 0.0;
  double deriv_x = 0.0;
  double deriv_y = 0.0;
  double deriv_z = 0.0;
  double deriv_norm_courb_para = 0.0;
  double delta_para = 0.0;
  double absci = 0.0;
  double delta_absci = 0.0;
  double para_bezier = 0.0;
  std::vector<double> absci_vec;
  std::vector<double> para_vec;

 //  --------------------------- Using soft Motion to generate the trajectory along the path
  Lac.resize(path.back().subpath.size());

  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    if(iter->type == LINE_TH) {
      lllength = sqrt((iter->line.end.x - iter->line.start.x)*(iter->line.end.x - iter->line.start.x) + (iter->line.end.y - iter->line.start.y)*(iter->line.end.y - iter->line.start.y));
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LINE){
      lllength = sqrt((iter->end.x - iter->start.x)*(iter->end.x - iter->start.x) + (iter->end.y - iter->start.y)*(iter->end.y - iter->start.y));
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == BEZIER3){
      lllength = bezier_length (iter->start, iter->bezier3[0], iter->bezier3[1] , iter->end, step_for_bezier);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == SINUS){
      lllength = length_sinus(path, indice_trace);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == CERCLE){
      lllength = length_cercle(path);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == PARABOL){
      lllength = length_parabol(path);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_L1){
      lllength = 1.0;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_C1){
      lllength = PI*iter->lccl.cercle1_rayon/2;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_C2){
      lllength = PI*iter->lccl.cercle2_rayon;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_L2){
      lllength = 1.0;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    i++;
  }


  ICloc.a = 0.0;
  ICloc.v = 0.0;
  ICloc.x = 0.0;

  FCloc.a = 0.0;
  FCloc.v = 0.0;
  FCloc.x = TotalLength;

  if (sm_ComputeSoftMotionLocal(ICloc, FCloc, Lim, &TimeSeg, &TrajType, &dcOut, &zoneOut) != 0) {
    return SM_ERROR;
  }

  //--------------------------- Compute u du ddu ---------------------------------------------

  IC[0] = ICloc.a;
  IC[1] = ICloc.v;
  IC[2] = ICloc.x;

  Time[0] = TimeSeg.Tjpa;
  Time[1] = TimeSeg.Taca;
  Time[2] = TimeSeg.Tjna;
  Time[3] = TimeSeg.Tvc;
  Time[4] = TimeSeg.Tjnb;
  Time[5] = TimeSeg.Tacb;
  Time[6] = TimeSeg.Tjpb;

  J[0] = Lim.maxJerk;
  J[1] = 0.0;
  J[2] = - Lim.maxJerk;
  J[3] = 0.0;
  J[4] = - Lim.maxJerk;
  J[5] = 0.0;
  J[6] = Lim.maxJerk;

  SM_LOG(SM_LOG_INFO, "Limits: " << Lim.maxJerk << ", "<< Lim.maxAcc << ", " << Lim.maxVel);

  for (i = 0; i < 7; i++){
    total_time = total_time + Time[i]; // total_time is the time for 7 segment
  }

  nbPoints = ((int) (total_time/tic)) + 1; // nbPoints is the point discretized by step of 0.001
  SM_LOG(SM_LOG_INFO, "tic = " << tic << " -- nbpoint = " <<  nbPoints << " -- totaltime = " << total_time);
  IdealTraj.resize(nbPoints);
  for(unsigned int h=0; h<IdealTraj.size(); h++) {
    IdealTraj[h].Pos.resize(3);
    IdealTraj[h].Vel.resize(3);
    IdealTraj[h].Acc.resize(3);
    IdealTraj[h].Jerk.resize(3);
  }
  ddu.resize(nbPoints);
  du.resize(nbPoints);
  u.resize(nbPoints);
  t.resize(nbPoints);

  for (i = 0; i < IdealTraj.size(); i++){
    IdealTraj[i].t = i * tic;
    if (IdealTraj[i].t >= total_time) {
      IdealTraj[i].t =total_time;
    }
    t.at(i) = IdealTraj[i].t;
  }

  sm_AVX_TimeVar(IC, Time, J, t, ddu, du, u);
  for (int i = 0; i < nbPoints; i++){
    IdealTraj[i].u_Mlaw = u[i];
    IdealTraj[i].du_Mlaw = du[i];
    IdealTraj[i].ddu_Mlaw = ddu[i];
  }

  // --------------------------- 3D evolution ---------------------------------------------

  j = 0;
  i = 0;
  double x1,x2,x3,x4;
  double y1,y2,y3,y4;
  /* This loop calculate the sum of parameters for each sous-path */
  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    para_bezier = 0.0;

    do {
      if(iter->type == BEZIER3) {
        x1 = iter->start.x;
        x2 = iter->bezier3[0].x;
        x3 = iter->bezier3[1].x;
        x4 = iter->end.x;
        y1 = iter->start.y;
        y2 = iter->bezier3[0].y;
        y3 = iter->bezier3[1].y;
        y4 = iter->end.y;
        deriv_x = -3.0*iter->start.x*(1.0-para_bezier)*(1.0-para_bezier) +
                  3.0*iter->bezier3[0].x*(1.0-para_bezier)*(1.0-para_bezier) - 6*iter->bezier3[0].x*para_bezier*(1.0-para_bezier) +
                  6.0*iter->bezier3[1].x*(1.0-para_bezier)*para_bezier - 3.0*iter->bezier3[1].x*para_bezier*para_bezier +
                  3.0*para_bezier*para_bezier*iter->end.x;
        deriv_y = -3.0*iter->start.y*(1.0-para_bezier)*(1.0-para_bezier) +
                  3.0*iter->bezier3[0].y*(1.0-para_bezier)*(1.0-para_bezier) - 6*iter->bezier3[0].y*para_bezier*(1.0-para_bezier) +
                  6.0*iter->bezier3[1].y*(1.0-para_bezier)*para_bezier - 3.0*iter->bezier3[1].y*para_bezier*para_bezier +
                  3.0*para_bezier*para_bezier*iter->end.y;
	deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y);
	delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
	para_bezier += delta_para;
      }
      if(iter->type == LINE){
        x1 = iter->start.x;
        x4 = iter->end.x;
        y1 = iter->start.y;
        y4 = iter->end.y;
	deriv_x = iter->end.x - iter->start.x;
	deriv_y = deriv_x * (iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
	deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y);
	delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
	para_bezier += delta_para;
      }
      j++;
      if(j >= u.size()) {
	break;
      }
    } while ((u.at(j) < Lac.at(i)));


    para_vec.push_back(para_bezier);
    i++;
    if(i >= Lac.size()) {
	break;
    }
  }

  j = 0;
  i = 0;

  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    para = 0.0;
    do {
      if (iter->type == BEZIER3){
        lpoint = bezier_point(para/para_vec[i], iter->start, iter->bezier3[0], iter->bezier3[1], iter->end);
        pos_x = lpoint.x;
        pos_y = lpoint.y;
        pos_z = 0.0;
        deriv_x = -3.0*iter->start.x*(1.0-para)*(1.0-para) +
                  3.0*iter->bezier3[0].x*(1.0-para)*(1.0-para) - 6*iter->bezier3[0].x*para*(1.0-para) +
                  6.0*iter->bezier3[1].x*(1.0-para)*para - 3.0*iter->bezier3[1].x*para*para +
                  3.0*para*para*iter->end.x;
        deriv_y = -3.0*iter->start.y*(1.0-para)*(1.0-para) +
                  3.0*iter->bezier3[0].y*(1.0-para)*(1.0-para) - 6*iter->bezier3[0].y*para*(1.0-para) +
                  6.0*iter->bezier3[1].y*(1.0-para)*para - 3.0*iter->bezier3[1].y*para*para +
                  3.0*para*para*iter->end.y;
        deriv_z = 0.0;
      }

      if(iter->type == LINE) {
	pos_x = iter->start.x + (para/para_vec[i])*(iter->end.x - iter->start.x);;
	pos_y = iter->start.y + (pos_x - iter->start.x)*(iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
    pos_z = 0.0;
	deriv_x = (iter->end.x - iter->start.x)/para_vec[i];
	deriv_y = deriv_x * (iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
    deriv_z = 0.0;
      }

      if(iter->type == LINE_TH) {
        pos_x = para;
        pos_y = iter->line.start.y + (para - iter->line.start.x)*(iter->line.end.y - iter->line.start.y)/(iter->line.end.x - iter->line.start.x);
        pos_z = 0.0;
        deriv_x = 1.0;
        deriv_y = (iter->line.end.y - iter->line.start.y)/(iter->line.end.x - iter->line.start.x);
        deriv_z = 0.0;
      }

      if(iter->type == CERCLE){
        pos_x = iter->cercle.radius * sin(2*PI*iter->cercle.sinus_para.frequency*para + PI/2) -  iter->cercle.radius; // bidouille
        pos_y = iter->cercle.radius * sin(2*PI*iter->cercle.sinus_para.frequency*para);
        pos_z = 0.0;
        deriv_x = 2*PI*iter->cercle.sinus_para.frequency*iter->cercle.radius *
                  cos(2*PI*iter->cercle.sinus_para.frequency*para + PI/2);
        deriv_y = 2*PI*iter->cercle.sinus_para.frequency*iter->cercle.radius *
                  cos(2*PI*iter->cercle.sinus_para.frequency*para);
        deriv_z = 0.0;
      }

      if(iter->type == SINUS){
        pos_x = para;
        pos_y = iter->sinus.amplitude *
                sin(2 * PI * iter->sinus.frequency * pos_x + iter->sinus.phase);
        pos_z = 0.0;
        deriv_x = 1.0;
        deriv_y = iter->sinus.amplitude *2 * PI * iter->sinus.frequency *
                  cos(2 * PI * iter->sinus.frequency * pos_x + iter->sinus.phase);
        deriv_z = 0.0;
      }

      if(iter->type == PARABOL){
        pos_x = para;
        pos_y = iter->parabol.a * pos_x * pos_x;
        deriv_x = 1.0;
        deriv_y = 2 * iter->parabol.a * pos_x;
      }

      if(iter->type == LCCL_L1){
	pos_x = para;
	pos_y = 0.0;
	pos_z = 0.0;
	deriv_x = 1.0;
	deriv_y = 0.0;
	deriv_z = 0.0;
      }

      if(iter->type == LCCL_C1){
	pos_x = iter->lccl.cercle1_rayon * sin(para - PI/2 + PI/2) + 1;
	pos_y = iter->lccl.cercle1_rayon * sin(para - PI/2) + iter->lccl.cercle1_rayon;
	pos_z = 0.0;
	deriv_x = iter->lccl.cercle1_rayon * cos(para - PI/2 + PI/2);
	deriv_y = iter->lccl.cercle1_rayon * cos(para - PI/2);
	deriv_z = 0.0;
      }

      if(iter->type == LCCL_C2){
	pos_x = 2.0;
	pos_y = -iter->lccl.cercle2_rayon * sin(-(para + PI/2) + PI/2) + 1;
	pos_z = iter->lccl.cercle2_rayon * sin(-(para + PI/2)) + 1;
	deriv_x = 0.0;
	deriv_y = iter->lccl.cercle2_rayon * cos(-(para + PI/2) + PI/2);
	deriv_z = -iter->lccl.cercle2_rayon * cos(-(para + PI/2));
      }

      if(iter->type == LCCL_L2){
	pos_x = 2.0;
	pos_y = 1.0 - para;
	pos_z = 2.0;
	deriv_x = 0.0;
	deriv_y = -1.0;
	deriv_z = 0.0;
      }

      /* Calculation of abscis and velocity for a trajectory */
      deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y + deriv_z * deriv_z);
      delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
      delta_absci = delta_para * deriv_norm_courb_para;
      absci += delta_absci;
      IdealTraj[j].absci = absci;
      IdealTraj[j].Pos[0] = pos_x;
      IdealTraj[j].Pos[1] = pos_y;
      IdealTraj[j].Pos[2] = pos_z;
      IdealTraj[j].Vel[0] = deriv_x * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      IdealTraj[j].Vel[1] = deriv_y * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      IdealTraj[j].Vel[2] = deriv_z * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      absci_vec.push_back(absci);
      para += delta_para;
      j++;
      if(j >= u.size()) {
	break;
      }

    }  while ((u.at(j) < Lac.at(i)));
    i++;
    if(i >= Lac.size()) {
	break;
    }
  }

  for (int i = 0; i < nbPoints; i++){
    IdealTraj[i].u  =  absci_vec[i];
    IdealTraj[i].du  =  sqrt((IdealTraj[i].Vel[0])*(IdealTraj[i].Vel[0]) + (IdealTraj[i].Vel[1])*(IdealTraj[i].Vel[1]) + (IdealTraj[i].Vel[2])*(IdealTraj[i].Vel[2]));
  }

  IdealTraj[IdealTraj.size()-1].ddu = 0.0;
  for (int i = 1; i < nbPoints; i++){
        IdealTraj[i-1].Acc[0] = (IdealTraj[i].Vel[0] - IdealTraj[i-1].Vel[0]) / (t[i] - t[i-1]);
        IdealTraj[i-1].Acc[1] = (IdealTraj[i].Vel[1] - IdealTraj[i-1].Vel[1]) / (t[i] - t[i-1]);
        IdealTraj[i-1].Acc[2] = (IdealTraj[i].Vel[2] - IdealTraj[i-1].Vel[2]) / (t[i] - t[i-1]);

        IdealTraj[i-1].AccNorm = sqrt((IdealTraj[i-1].Acc[0])*(IdealTraj[i-1].Acc[0]) + (IdealTraj[i-1].Acc[1])*(IdealTraj[i-1].Acc[1]) + (IdealTraj[i-1].Acc[2])*(IdealTraj[i-1].Acc[2]));
        IdealTraj[i-1].ddu  = sqrt((IdealTraj[i-1].Acc[0])*(IdealTraj[i-1].Acc[0]) + (IdealTraj[i-1].Acc[1])*(IdealTraj[i-1].Acc[1]) + (IdealTraj[i-1].Acc[2])*(IdealTraj[i-1].Acc[2]));
  }

  return SM_OK;
}



SM_STATUS constructTrajSvg(std::list<Path> &path, double tic, SM_LIMITS Lim, std::vector<SM_CURVE_DATA3> &IdealTraj)
{

  //SM_LINE_ARC PathComp[], SM_ROT Rot[], int nbComp, double PosInit[3], SM_LIMITS Lim, double tic, int *nbPoints, SM_CURVE_DATA IdealTraj[SM_NB_DISC_MAX]) {

//  This function take a serie of lines and arcs (using Euler spiral parameter)  and convert into a discretized trajectory
//      -------- PathComp[] : Line & Arc definition
//      -------- Rot[]      : Rotation Information (Rotation matrix)
//      -------- nbComp     : total number of line or arc
//      -------- PosInit    : Stating Position of the path
//      -------- tic        : Time step for discretization (in second)
//      -------- nbPoints   : Output number of sample points (echantillonage)
//      -------- IdealTraj[]: Discretized output information of the input curve


  std::list<SubPath>::iterator iter;
  double lllength = 0.0;
  int nbPoints = 0;
  unsigned int i = 0;
  unsigned int j = 0;
  int TrajType = 0;
  int zoneOut = 0;
  double TotalLength = 0.0;
  double dcOut = 0.0;
  SM_COND ICloc, FCloc;
  SM_TIMES TimeSeg;

  std::vector<double> Time;
  Time.resize(7);
  std::vector<double> IC;
  IC.resize(3);
  std::vector<double> J;
  J.resize(7);
  std::vector<double> t, ddu, du, u;
//    double *t = NULL, *ddu =  NULL, *du = NULL, *u = NULL;
  double total_time = 0.0;
   std::vector<double> Lac ;
  Point2D lpoint;
  std::vector<double> dis_pos;
  std::vector<double> distance_tal;
  std::vector<IndiceTrace> indice_trace;

  double pos_x = 0.0;
  double pos_y = 0.0;
  double pos_z = 0.0;
  double para = 0.0;
  double deriv_x = 0.0;
  double deriv_y = 0.0;
  double deriv_z = 0.0;
  double deriv_norm_courb_para = 0.0;
  double delta_para = 0.0;
  double absci = 0.0;
  double delta_absci = 0.0;
  double para_bezier = 0.0;
  std::vector<double> absci_vec;
  std::vector<double> para_vec;

 //  --------------------------- Using soft Motion to generate the trajectory along the path
  Lac.resize(path.back().subpath.size());

  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    if(iter->type == LINE_TH) {
      lllength = sqrt((iter->line.end.x - iter->line.start.x)*(iter->line.end.x - iter->line.start.x) + (iter->line.end.y - iter->line.start.y)*(iter->line.end.y - iter->line.start.y));
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LINE){
      lllength = sqrt((iter->end.x - iter->start.x)*(iter->end.x - iter->start.x) + (iter->end.y - iter->start.y)*(iter->end.y - iter->start.y));
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == BEZIER3){
      lllength = bezier_length (iter->start, iter->bezier3[0], iter->bezier3[1] , iter->end, step_for_bezier);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == SINUS){
      lllength = length_sinus(path, indice_trace);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if (iter->type == CERCLE){
      lllength = length_cercle(path);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == PARABOL){
      lllength = length_parabol(path);
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_L1){
      lllength = 1.0;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_C1){
      lllength = PI*iter->lccl.cercle1_rayon/2;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_C2){
      lllength = PI*iter->lccl.cercle2_rayon;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    else if(iter->type == LCCL_L2){
      lllength = 1.0;
      TotalLength += lllength;
      Lac.at(i) = TotalLength;
    }
    i++;
  }


  ICloc.a = 0.0;
  ICloc.v = 0.0;
  ICloc.x = 0.0;

  FCloc.a = 0.0;
  FCloc.v = 0.0;
  FCloc.x = TotalLength;

  if (sm_ComputeSoftMotionLocal(ICloc, FCloc, Lim, &TimeSeg, &TrajType, &dcOut, &zoneOut) != 0) {
    return SM_ERROR;
  }

  //--------------------------- Compute u du ddu ---------------------------------------------

  IC[0] = ICloc.a;
  IC[1] = ICloc.v;
  IC[2] = ICloc.x;

  Time[0] = TimeSeg.Tjpa;
  Time[1] = TimeSeg.Taca;
  Time[2] = TimeSeg.Tjna;
  Time[3] = TimeSeg.Tvc;
  Time[4] = TimeSeg.Tjnb;
  Time[5] = TimeSeg.Tacb;
  Time[6] = TimeSeg.Tjpb;

  J[0] = Lim.maxJerk;
  J[1] = 0.0;
  J[2] = - Lim.maxJerk;
  J[3] = 0.0;
  J[4] = - Lim.maxJerk;
  J[5] = 0.0;
  J[6] = Lim.maxJerk;


  SM_LOG(SM_LOG_INFO, "Limits: " << Lim.maxJerk << ", " << Lim.maxAcc << ", " << Lim.maxVel);

  for (i = 0; i < 7; i++){
    total_time = total_time + Time[i]; // total_time is the time for 7 segment
  }

  nbPoints = ((int) (total_time/tic)) + 1; // nbPoints is the point discretized by step of 0.001
  SM_LOG(SM_LOG_INFO, "tic = " << tic << " -- nbpoint = " <<  nbPoints << " -- totaltime = " << total_time);
  IdealTraj.resize(nbPoints);
//  for(unsigned int h=0; h<IdealTraj.size(); h++) {
//    IdealTraj[h].Pos.resize(3);
//    IdealTraj[h].Vel.resize(3);
//    IdealTraj[h].Acc.resize(3);
//    IdealTraj[h].Jerk.resize(3);
//  }
  ddu.resize(nbPoints);
  du.resize(nbPoints);
  u.resize(nbPoints);
  t.resize(nbPoints);

  for (i = 0; i < IdealTraj.size(); i++){
    IdealTraj[i].t = i * tic;
    if (IdealTraj[i].t >= total_time) {
      IdealTraj[i].t =total_time;
    }
    t.at(i) = IdealTraj[i].t;
  }
  //printf("IC %f %f %f\n", IC[0],  IC[1],  IC[2]);
  //
  //printf("Time %f %f %f %f %f %f %f\n",Time[0], Time[1], Time[2], Time[3],Time[4], Time[5], Time[6]);
  //printf("J %f %f %f %f %f %f %f\n",J[0], J[1], J[2], J[3], J[4], J[5], J[6]);

  sm_AVX_TimeVar(IC, Time, J, t, ddu, du, u);
  for (int i = 0; i < nbPoints; i++){
    IdealTraj[i].u_Mlaw = u[i];
    IdealTraj[i].du_Mlaw = du[i];
    IdealTraj[i].ddu_Mlaw = ddu[i];
    //    printf("%f \n", ddu[i]);
  }

  // --------------------------- 3D evolution ---------------------------------------------

  j = 0;
  i = 0;
  double x1,x2,x3,x4;
  double y1,y2,y3,y4;
  /* This loop calculate the sum of parameters for each sous-path */
  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    para_bezier = 0.0;

    do {
      if(iter->type == BEZIER3) {
        x1 = iter->start.x;
        x2 = iter->bezier3[0].x;
        x3 = iter->bezier3[1].x;
        x4 = iter->end.x;
        y1 = iter->start.y;
        y2 = iter->bezier3[0].y;
        y3 = iter->bezier3[1].y;
        y4 = iter->end.y;
        deriv_x = -3.0*iter->start.x*(1.0-para_bezier)*(1.0-para_bezier) +
                  3.0*iter->bezier3[0].x*(1.0-para_bezier)*(1.0-para_bezier) - 6*iter->bezier3[0].x*para_bezier*(1.0-para_bezier) +
                  6.0*iter->bezier3[1].x*(1.0-para_bezier)*para_bezier - 3.0*iter->bezier3[1].x*para_bezier*para_bezier +
                  3.0*para_bezier*para_bezier*iter->end.x;
        deriv_y = -3.0*iter->start.y*(1.0-para_bezier)*(1.0-para_bezier) +
                  3.0*iter->bezier3[0].y*(1.0-para_bezier)*(1.0-para_bezier) - 6*iter->bezier3[0].y*para_bezier*(1.0-para_bezier) +
                  6.0*iter->bezier3[1].y*(1.0-para_bezier)*para_bezier - 3.0*iter->bezier3[1].y*para_bezier*para_bezier +
                  3.0*para_bezier*para_bezier*iter->end.y;
	deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y);
	delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
	para_bezier += delta_para;
      }
      if(iter->type == LINE){
        x1 = iter->start.x;
        x4 = iter->end.x;
        y1 = iter->start.y;
        y4 = iter->end.y;
	deriv_x = iter->end.x - iter->start.x;
	deriv_y = deriv_x * (iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
	deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y);
	delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
	para_bezier += delta_para;
      }
      j++;
      if(j >= u.size()) {
	break;
      }
    } while ((u.at(j) < Lac.at(i)));


    para_vec.push_back(para_bezier);
    i++;
    if(i >= Lac.size()) {
	break;
    }
  }

  j = 0;
  i = 0;

  for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
    para = 0.0;
    do {
      if (iter->type == BEZIER3){
        lpoint = bezier_point(para/para_vec[i], iter->start, iter->bezier3[0], iter->bezier3[1], iter->end);
        pos_x = lpoint.x;
        pos_y = lpoint.y;
        pos_z = 0.0;
        deriv_x = -3.0*iter->start.x*(1.0-para)*(1.0-para) +
                  3.0*iter->bezier3[0].x*(1.0-para)*(1.0-para) - 6*iter->bezier3[0].x*para*(1.0-para) +
                  6.0*iter->bezier3[1].x*(1.0-para)*para - 3.0*iter->bezier3[1].x*para*para +
                  3.0*para*para*iter->end.x;
        deriv_y = -3.0*iter->start.y*(1.0-para)*(1.0-para) +
                  3.0*iter->bezier3[0].y*(1.0-para)*(1.0-para) - 6*iter->bezier3[0].y*para*(1.0-para) +
                  6.0*iter->bezier3[1].y*(1.0-para)*para - 3.0*iter->bezier3[1].y*para*para +
                  3.0*para*para*iter->end.y;
        deriv_z = 0.0;
      }

      if(iter->type == LINE) {
	pos_x = iter->start.x + (para/para_vec[i])*(iter->end.x - iter->start.x);;
	pos_y = iter->start.y + (pos_x - iter->start.x)*(iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
    pos_z = 0.0;
	deriv_x = (iter->end.x - iter->start.x)/para_vec[i];
	deriv_y = deriv_x * (iter->end.y - iter->start.y)/(iter->end.x - iter->start.x);
    deriv_z = 0.0;
      }

      if(iter->type == LINE_TH) {
        pos_x = para;
        pos_y = iter->line.start.y + (para - iter->line.start.x)*(iter->line.end.y - iter->line.start.y)/(iter->line.end.x - iter->line.start.x);
        pos_z = 0.0;
        deriv_x = 1.0;
        deriv_y = (iter->line.end.y - iter->line.start.y)/(iter->line.end.x - iter->line.start.x);
        deriv_z = 0.0;
      }

      if(iter->type == CERCLE){
        pos_x = iter->cercle.radius * sin(2*PI*iter->cercle.sinus_para.frequency*para + PI/2) -  iter->cercle.radius; // bidouille
        pos_y = iter->cercle.radius * sin(2*PI*iter->cercle.sinus_para.frequency*para);
        pos_z = 0.0;
        deriv_x = 2*PI*iter->cercle.sinus_para.frequency*iter->cercle.radius *
                  cos(2*PI*iter->cercle.sinus_para.frequency*para + PI/2);
        deriv_y = 2*PI*iter->cercle.sinus_para.frequency*iter->cercle.radius *
                  cos(2*PI*iter->cercle.sinus_para.frequency*para);
        deriv_z = 0.0;
      }

      if(iter->type == SINUS){
        pos_x = para;
        pos_y = iter->sinus.amplitude *
                sin(2 * PI * iter->sinus.frequency * pos_x + iter->sinus.phase);
        pos_z = 0.0;
        deriv_x = 1.0;
        deriv_y = iter->sinus.amplitude *2 * PI * iter->sinus.frequency *
                  cos(2 * PI * iter->sinus.frequency * pos_x + iter->sinus.phase);
        deriv_z = 0.0;
      }

      if(iter->type == PARABOL){
        pos_x = para;
        pos_y = iter->parabol.a * pos_x * pos_x;
        deriv_x = 1.0;
        deriv_y = 2 * iter->parabol.a * pos_x;
      }

      if(iter->type == LCCL_L1){
	pos_x = para;
	pos_y = 0.0;
	pos_z = 0.0;
	deriv_x = 1.0;
	deriv_y = 0.0;
	deriv_z = 0.0;
      }

      if(iter->type == LCCL_C1){
	pos_x = iter->lccl.cercle1_rayon * sin(para - PI/2 + PI/2) + 1;
	pos_y = iter->lccl.cercle1_rayon * sin(para - PI/2) + iter->lccl.cercle1_rayon;
	pos_z = 0.0;
	deriv_x = iter->lccl.cercle1_rayon * cos(para - PI/2 + PI/2);
	deriv_y = iter->lccl.cercle1_rayon * cos(para - PI/2);
	deriv_z = 0.0;
      }

      if(iter->type == LCCL_C2){
	pos_x = 2.0;
	pos_y = -iter->lccl.cercle2_rayon * sin(-(para + PI/2) + PI/2) + 1;
	pos_z = iter->lccl.cercle2_rayon * sin(-(para + PI/2)) + 1;
	deriv_x = 0.0;
	deriv_y = iter->lccl.cercle2_rayon * cos(-(para + PI/2) + PI/2);
	deriv_z = -iter->lccl.cercle2_rayon * cos(-(para + PI/2));
      }

      if(iter->type == LCCL_L2){
	pos_x = 2.0;
	pos_y = 1.0 - para;
	pos_z = 2.0;
	deriv_x = 0.0;
	deriv_y = -1.0;
	deriv_z = 0.0;
      }

      /* Calculation of abscis and velocity for a trajectory */
      deriv_norm_courb_para = sqrt(deriv_x * deriv_x + deriv_y * deriv_y + deriv_z * deriv_z);
      delta_para = IdealTraj[j].du_Mlaw * tic / deriv_norm_courb_para;
      delta_absci = delta_para * deriv_norm_courb_para;
      absci += delta_absci;
      IdealTraj[j].absci = absci;
      IdealTraj[j].Pos[0] = pos_x;
      IdealTraj[j].Pos[1] = pos_y;
      IdealTraj[j].Pos[2] = pos_z;
      IdealTraj[j].Vel[0] = deriv_x * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      IdealTraj[j].Vel[1] = deriv_y * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      IdealTraj[j].Vel[2] = deriv_z * IdealTraj[j].du_Mlaw / deriv_norm_courb_para;
      absci_vec.push_back(absci);
      para += delta_para;
      j++;
      if(j >= u.size()) {
	break;
      }

    }  while ((u.at(j) < Lac.at(i)));
    i++;
    if(i >= Lac.size()) {
	break;
    }
  }

  for (int i = 0; i < nbPoints; i++){
    IdealTraj[i].u  =  absci_vec[i];
    IdealTraj[i].du  =  sqrt((IdealTraj[i].Vel[0])*(IdealTraj[i].Vel[0]) + (IdealTraj[i].Vel[1])*(IdealTraj[i].Vel[1]) + (IdealTraj[i].Vel[2])*(IdealTraj[i].Vel[2]));
  }

  IdealTraj[IdealTraj.size()-1].ddu = 0.0;
  for (int i = 1; i < nbPoints; i++){
        IdealTraj[i-1].Acc[0] = (IdealTraj[i].Vel[0] - IdealTraj[i-1].Vel[0]) / (t[i] - t[i-1]);
        IdealTraj[i-1].Acc[1] = (IdealTraj[i].Vel[1] - IdealTraj[i-1].Vel[1]) / (t[i] - t[i-1]);
        IdealTraj[i-1].Acc[2] = (IdealTraj[i].Vel[2] - IdealTraj[i-1].Vel[2]) / (t[i] - t[i-1]);

        IdealTraj[i-1].AccNorm = sqrt((IdealTraj[i-1].Acc[0])*(IdealTraj[i-1].Acc[0]) + (IdealTraj[i-1].Acc[1])*(IdealTraj[i-1].Acc[1]) + (IdealTraj[i-1].Acc[2])*(IdealTraj[i-1].Acc[2]));
        IdealTraj[i-1].ddu  = sqrt((IdealTraj[i-1].Acc[0])*(IdealTraj[i-1].Acc[0]) + (IdealTraj[i-1].Acc[1])*(IdealTraj[i-1].Acc[1]) + (IdealTraj[i-1].Acc[2])*(IdealTraj[i-1].Acc[2]));
  }

  return SM_OK;
}





void wait_for_key ()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)  // every keypress registered, also arrow keys
  SM_LOG(SM_LOG_INFO,  endl << "Press any key to continue...");

  FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
  _getch();
#elif defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__)



 int temps = 3;
 SM_LOG(SM_LOG_INFO,  endl<< "Wait "<< temps << " seconds");
 clock_t arrivee=clock()+(temps*CLOCKS_PER_SEC); // On calcule le moment ou l'attente devra s'arreter
 while(clock()<arrivee);

    //   std::cout << std::endl << "Press ENTER to continue..." << std::endl;
   // std::cin.clear(); //on clear le buffer d'entrer
   // std::cin.ignore(std::cin.rdbuf()->in_avail(), '\n'); //on ignore toute les touches qui sont taper avant la touche entrer
   //std::cin.get(); // on


#endif
  return;
}


//int main(int argc, char **argv){
//
//
//  std::vector<SM_OUTPUT> motion;
//  //SM_OUTPUT motion[SM_NB_INTERVALS_MAX];
//  int nbJerkConst;
//
//  FILE *ofp;
//  char outputFilename[] = "out.txt";
//  int i;
//  std::string fileName;
//
//  ofp = fopen(outputFilename, "w");
//
//  if (ofp == NULL) {
//    fprintf(stderr, "Can't open output file %s!\n", outputFilename);
//  }
//
//  fileName.clear();
//  fileName.append(argv[1]);
//  sm_Main(motion, &nbJerkConst, fileName);
//  printf("Approximation OK --> nbJekConst = %d\n",nbJerkConst);
//
//
//  fprintf(ofp, "Jx = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Jerk[0]);
//  }
//  fprintf(ofp, "] \n Tx = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Time[0]);
//  }
//
//  fprintf(ofp, "] \n");
//  fprintf(ofp, "Jy = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Jerk[1]);
//  }
//  fprintf(ofp, "] \n Ty = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Time[1]);
//  }
//
//
//  fprintf(ofp, "] \n Z");
//  fprintf(ofp, "Jz = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Jerk[2]);
//  }
//  fprintf(ofp, "] \n Tz = [");
//  for (i = 0; i < nbJerkConst; i++){
//    fprintf(ofp,"%lf ", motion[i].Time[2]);
//  }
//  fprintf(ofp, "]");
//  fclose(ofp);
//
//  return 0;
//
//}
//


SM_STATUS Calcul_Error(std::vector<SM_CURVE_DATA>  &IdealTraj,std::vector<SM_CURVE_DATA> &ApproxTraj, kinPoint *errorMax, std::vector<double>& error, double *errMax_pos){
    error.clear();
    double err;
    for (unsigned int i = 0; i< ApproxTraj.size(); i++){
      double dist2 = 0;
     
//       for(unsigned int h =0; h<IdealTraj[0].Pos.size(); h++) {
// 	dist2 += (IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h])*(IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h]);
//       }
//       err  = sqrt(dist2);


        err = 0.0;
      for(unsigned int h =0; h<IdealTraj[0].Pos.size(); h++) {
	dist2 = ABS(IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h]);
	if(dist2>err) {
	   err = dist2;
	}
      }
      if(err > *errMax_pos) {
	errorMax->kc.clear();
	errorMax->kc.resize(IdealTraj[i].Pos.size());
	for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
	  errorMax->kc[h].x =  IdealTraj[i].Pos[h];
	}
	errorMax->t= IdealTraj[i].t;
	  *errMax_pos = err;
      }
      error.push_back(err);
    }

  return SM_OK;
}

SM_STATUS Calcul_Error(std::vector<SM_CURVE_DATA3>  &IdealTraj,std::vector<SM_CURVE_DATA3> &ApproxTraj, kinPoint3 *errorMax, std::vector<double>& error, double *val_err_max){
    error.clear();
    double err;
    for (unsigned int i = 0; i< ApproxTraj.size(); i++){

        err = sqrt(
              (IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])*(IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0 ])+
              (IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])*(IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1 ])+
              (IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2])*(IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2 ]));
              
        if(err > *val_err_max) {
        errorMax->kc[0].x= IdealTraj[i].Pos[0];
        errorMax->kc[1].x= IdealTraj[i].Pos[1];
        errorMax->kc[2].x= IdealTraj[i].Pos[2];
        errorMax->t= IdealTraj[i].t;
        *val_err_max = err;
        }
        error.push_back(err);
    }

  return SM_OK;
}


SM_STATUS Calcul_Error_Vilocity(std::vector<SM_CURVE_DATA>  &IdealTraj,std::vector<SM_CURVE_DATA> &ApproxTraj, std::vector<double>& error_vit, double *errMax){
  error_vit.clear();
  double err = 0.0;
  for (unsigned int i = 0; i< ApproxTraj.size(); i++){
    err = IdealTraj[i].du - ApproxTraj[i].du;
  }
  if (err > *errMax){
    *errMax = err;
  }
  error_vit.push_back(err);

  return SM_OK;
}


SM_STATUS Calcul_Error_nw(std::vector<SM_CURVE_DATA>  &IdealTraj,std::vector<SM_CURVE_DATA> &ApproxTraj, std::vector<double>& error, double *errMax_pos){
    error.clear();
    double err;
    for (unsigned int i = 0; i< ApproxTraj.size(); i++){

        err = sqrt(
              (IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])*(IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0 ])+
              (IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])*(IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1 ])+
              (IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2])*(IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2 ]));

        if(err > *errMax_pos) {
        *errMax_pos = err;
        }
        error.push_back(err);
    }

  return SM_OK;
}


SM_STATUS Vel_Profile(std::vector<SM_CURVE_DATA>  &IdealTraj, std::vector<double> &vel_discr_X,std::vector<double> &vel_discr_Y, std::vector<double> &acc_discr_X, std::vector<double> &acc_discr_Y, std::vector<double> &pos_discr_X, std::vector<double> &pos_discr_Y){
    for (unsigned int j=0;j<IdealTraj.size();j++){
        pos_discr_X.push_back(IdealTraj[j].Pos[0]);
        pos_discr_Y.push_back(IdealTraj[j].Pos[1]);
    }
    for (unsigned int j=0;j<IdealTraj.size();j++){
        vel_discr_X.push_back(IdealTraj[j].Vel[0]);
        vel_discr_Y.push_back(IdealTraj[j].Vel[1]);
    }
    for (unsigned int k = 0; k<vel_discr_X.size();k++){
        acc_discr_X.push_back(IdealTraj[k].Acc[0]);
        acc_discr_Y.push_back(IdealTraj[k].Acc[1]);
    }

    return SM_OK;
}

SM_STATUS Vel_Profile(std::vector<SM_CURVE_DATA3>  &IdealTraj, std::vector<double> &vel_discr_X,std::vector<double> &vel_discr_Y, std::vector<double> &acc_discr_X, std::vector<double> &acc_discr_Y, std::vector<double> &pos_discr_X, std::vector<double> &pos_discr_Y){
    for (unsigned int j=0;j<IdealTraj.size();j++){
        pos_discr_X.push_back(IdealTraj[j].Pos[0]);
        pos_discr_Y.push_back(IdealTraj[j].Pos[1]);
    }
    for (unsigned int j=0;j<IdealTraj.size();j++){
        vel_discr_X.push_back(IdealTraj[j].Vel[0]);
        vel_discr_Y.push_back(IdealTraj[j].Vel[1]);
    }
    for (unsigned int k = 0; k<vel_discr_X.size();k++){
        acc_discr_X.push_back(IdealTraj[k].Acc[0]);
        acc_discr_Y.push_back(IdealTraj[k].Acc[1]);
    }

    return SM_OK;
}

SM_STATUS Vel_Profile_Path(std::list<Path> &path, std::vector<double> &vel_path_x, std::vector<double> &vel_path_y, double sample_time){
    double sub_length;
    double t;
    double pos_x;
    double pos_y;
    double pos_pre_x;
    double pos_pre_y;
    double TotalLength=0.0;
    Point2D dot;
    Point2D previous_dot;

    std::list<SubPath>::iterator iter;

    for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
        if(iter->type == LINE) {
            double x_diff = (iter->end.x - iter->start.x);
            double y_diff = (iter->end.y - iter->start.y);
            pos_pre_x = iter->start.x;
            pos_pre_y = iter->start.y;
            for (t = sample_time; t<=1; t = t+sample_time){
                pos_x = iter->start.x + x_diff * t;
                pos_y = iter->start.y + y_diff * t;
                vel_path_x.push_back((pos_x-pos_pre_x)/sample_time);
                vel_path_y.push_back((pos_y-pos_pre_y)/sample_time);
                pos_pre_x = pos_x;
                pos_pre_y = pos_y;
            }
        }

        else if (iter->type == BEZIER3){
            sub_length = bezier_length (iter->start, iter->bezier3[0], iter->bezier3[1] , iter->end, step_for_bezier);
            TotalLength = sub_length + TotalLength;

            for (t = 0.0; t <= 1; t = t+sample_time) {
                dot = bezier_point (t, iter->start, iter->bezier3[0], iter->bezier3[1], iter->end);
                if (t > 0) {
                    double x_diff = dot.x - previous_dot.x;
                    vel_path_x.push_back(x_diff/sample_time);
                    double y_diff = dot.y - previous_dot.y;
                    vel_path_y.push_back(y_diff/sample_time);
                }
                previous_dot = dot;
            }
        }
    }
    return SM_OK;
}

SM_STATUS  Hausdorff(std::vector<SM_CURVE_DATA>  &idealTraj, std::vector<SM_CURVE_DATA>  &proxTraj, std::vector<double> &dis_a_tracer1, std::vector<double> &dis_a_tracer2, double *sup1, double *sup2){

    double dis_hausdorff;
    double w=0;
    // f1 pour calculer la distance la plus longue entre courbe1 et courbe2


    for (int i=0; i< (int)idealTraj.size(); i++){
        std::vector<double> dis1;

        for (int j=0; j<  (int)proxTraj.size(); j++){
	  w = sqrt(  pow(     (idealTraj[i].Pos[0]-proxTraj[j].Pos[0])  ,2) +  pow( (idealTraj[i].Pos[1]-proxTraj[j].Pos[1]),2 ) +      pow((idealTraj[i].Pos[2]-proxTraj[j].Pos[2]), 2));
            dis1.push_back (w);
        }

        double inf1 = dis1[0];

        for (int k=0; k<(int)proxTraj.size(); k++){
            if (dis1[k]<inf1) {inf1 = dis1[k];}
        }
        dis_a_tracer1.push_back(inf1);
    }

    *sup1 = dis_a_tracer1[0];

    for (int m=0; m<(int)idealTraj.size(); m++){
        if (dis_a_tracer1[m]>(*sup1)) {*sup1 = dis_a_tracer1[m];}
    }

// f2 pour calculer la distance la plus longue entre courbe2 et courbe1

    for (int i=0; i< (int)proxTraj.size(); i++){
        std::vector<double> dis2;

        for (int j=0; j< (int)idealTraj.size(); j++){
	  w = sqrt  (      pow(   (proxTraj[i].Pos[0]-idealTraj[j].Pos[0]),2) + pow((proxTraj[i].Pos[1]-idealTraj[j].Pos[1]),2 ) +
			   pow((proxTraj[i].Pos[2]-idealTraj[j].Pos[2]),2));

            dis2.push_back(w);
        }

        double inf2 = dis2[0];

        for (int k=0; k<(int)idealTraj.size(); k++){
            if (dis2[k]<inf2) {inf2 = dis2[k];}
        }
        dis_a_tracer2.push_back(inf2);
    }

    *sup2 = dis_a_tracer2[0];

    for (int m=0; m<(int)proxTraj.size() ; m++){
        if (dis_a_tracer2[m]>(*sup2)) {*sup2 = dis_a_tracer2[m];}
    }

// calcul de la distance hausdorff
    dis_hausdorff = (*sup1 > *sup2 ? *sup1 : *sup2);

    return SM_OK;
}


SM_STATUS Path_Length(std::list<Path> &path, double *longeur){
    std::list<SubPath>::iterator iter;
    double lllength = 0.0;
    unsigned int i = 0;
    double TotalLength = 0.0;
    double *Lac = NULL;
    Lac   = (double *) malloc(sizeof(double) * path.back().subpath.size());
    std::vector<IndiceTrace> indice_trace;
    for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {
      if(iter->type == LINE_TH) {
        lllength = sqrt((iter->line.end.x - iter->line.start.x)*(iter->line.end.x - iter->line.start.x) +
                (iter->line.end.y - iter->line.start.y)*(iter->line.end.y - iter->line.start.y));
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
      }
      if(iter->type == LINE) {
        lllength = sqrt((iter->end.x - iter->start.x)*(iter->end.x - iter->start.x) +
                (iter->end.y - iter->start.y)*(iter->end.y - iter->start.y));
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
      }
      else if (iter->type == BEZIER3){
        lllength = bezier_length (iter->start, iter->bezier3[0], iter->bezier3[1] , iter->end, step_for_bezier);
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
	  }
      else if (iter->type == SINUS){
        lllength = length_sinus(path, indice_trace);
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
      }
      else if(iter->type == CERCLE){
        lllength = length_cercle(path);
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
      }
      else if(iter->type == PARABOL){
        lllength = length_parabol(path);
        TotalLength += lllength;
        Lac[i] = TotalLength;
        *longeur = Lac[i];
      }
    i++;
    }
return SM_OK;
}

SM_STATUS Calcul_Error_list(std::vector<SM_CURVE_DATA>  &IdealTraj, std::vector<SM_CURVE_DATA>  &ApproxTraj, kinPoint *errorMax, std::vector<double>& error_pos, std::vector<double>& error_vel, double *errMax_pos, double *errMax_vel){
    error_pos.clear();
    error_vel.clear();
    double err = 0.0;
    *errMax_vel = 0.0;
     *errMax_pos = 0.0;
    for (unsigned int i = 0; i< IdealTraj.size(); i++){

     double dist2 = 0;
//       for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
// 	dist2 += (IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h])*(IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h]);
//       }
//       err  = sqrt(dist2);

err = 0.0;
      for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
	dist2 = ABS(IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h]);
	if(dist2>err) {
	   err = dist2;
	}
      }

      
      
      if(err > *errMax_pos) {
	errorMax->kc.clear();
	errorMax->kc.resize(IdealTraj[i].Pos.size());
	for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
	  errorMax->kc[h].x =  IdealTraj[i].Pos[h];
	}
	errorMax->t= IdealTraj[i].t;
	  *errMax_pos = err;
      }

    //    err = sqrt(
    //            (IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])*(IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])+
    //            (IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])*(IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])+
    //            (IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2])*(IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2]));
    //    if(err > *errMax_pos) {
    //        errorMax->kc[0].x= IdealTraj[i].Pos[0];
    //        errorMax->kc[1].x= IdealTraj[i].Pos[1];
    //        errorMax->kc[2].x= IdealTraj[i].Pos[2];
    //        errorMax->t= IdealTraj[i].t;
    //        *errMax_pos = err;
    //    }

        error_pos.push_back(err);
    }

    err = 0.0;
    for (unsigned int i = 0; i< IdealTraj.size(); i++){

      double dist2 = 0;
//       for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
// 	dist2 += (IdealTraj[i].Vel[h]-ApproxTraj[i].Vel[h])*(IdealTraj[i].Vel[h]-ApproxTraj[i].Vel[h]);
//       }
//       err  = sqrt(dist2);

      err = 0.0;
      for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
	dist2 = (IdealTraj[i].Vel[h]-ApproxTraj[i].Vel[h]);
	if(dist2>err) {
	   err = dist2;
	}
      }
      


       // err = sqrt(
       //           (IdealTraj[i].Vel[0]-ApproxTraj[i].Vel[0])*(IdealTraj[i].Vel[0]-ApproxTraj[i].Vel[0])+
       //           (IdealTraj[i].Vel[1]-ApproxTraj[i].Vel[1])*(IdealTraj[i].Vel[1]-ApproxTraj[i].Vel[1])+
       //           (IdealTraj[i].Vel[2]-ApproxTraj[i].Vel[2])*(IdealTraj[i].Vel[2]-ApproxTraj[i].Vel[2]));




        if(err > *errMax_vel) {
            *errMax_vel = err;
        }
        error_vel.push_back(err);
    }
    return SM_OK;
}

SM_STATUS Calcul_Error_list(std::vector<SM_CURVE_DATA3>  &IdealTraj, std::vector<SM_CURVE_DATA3>  &ApproxTraj, kinPoint3 *errorMax, std::vector<double>& error_pos, std::vector<double>& error_vel, double *errMax_pos, double *errMax_vel){
    error_pos.clear();
    error_vel.clear();
    double err = 0.0;
    *errMax_vel = 0.0;
     *errMax_pos = 0.0;
    for (unsigned int i = 0; i< IdealTraj.size(); i++){

     //double dist2 = 0;
     // for(unsigned int h =0; h<IdealTraj[0].Pos.size(); h++) {
     //   dist2 += (IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h])*(IdealTraj[i].Pos[h]-ApproxTraj[i].Pos[h]);
     // }
     // err  = sqrt(dist2);
     // �
     // if(err > *errMax_pos) {
     //   errorMax->kc.clear();
     //   errorMax->kc.resize(IdealTraj[i].Pos.size());
     //   for(unsigned int h =0; h<IdealTraj[i].Pos.size(); h++) {
     //     errorMax->kc[h].x =  IdealTraj[i].Pos[h];
     //   }
     //   errorMax->t= IdealTraj[i].t;
     //     *errMax_pos = err;
     // }

        err = sqrt(
                (IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])*(IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])+
                (IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])*(IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])+
                (IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2])*(IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2]));
        if(err > *errMax_pos) {
            errorMax->kc[0].x= IdealTraj[i].Pos[0];
            errorMax->kc[1].x= IdealTraj[i].Pos[1];
            errorMax->kc[2].x= IdealTraj[i].Pos[2];
            errorMax->t= IdealTraj[i].t;
            *errMax_pos = err;
        }

      //   error_pos.push_back(err);
    }

    err = 0.0;
    for (unsigned int i = 0; i< IdealTraj.size(); i++){

      //double dist2 = 0;
      //for(unsigned int h =0; h<IdealTraj[0].Pos.size(); h++) {
      //  dist2 += (IdealTraj[i].Vel[h]-ApproxTraj[i].Vel[h])*(IdealTraj[i].Vel[h]-ApproxTraj[i].Vel[h]);
      //}
      //err  = sqrt(dist2);
      


        err = sqrt(
                  (IdealTraj[i].Vel[0]-ApproxTraj[i].Vel[0])*(IdealTraj[i].Vel[0]-ApproxTraj[i].Vel[0])+
                  (IdealTraj[i].Vel[1]-ApproxTraj[i].Vel[1])*(IdealTraj[i].Vel[1]-ApproxTraj[i].Vel[1])+
                  (IdealTraj[i].Vel[2]-ApproxTraj[i].Vel[2])*(IdealTraj[i].Vel[2]-ApproxTraj[i].Vel[2]));




        if(err > *errMax_vel) {
            *errMax_vel = err;
        }
        error_vel.push_back(err);
    }
    return SM_OK;
}



SM_STATUS Calcul_Error_list_nw(std::vector<SM_CURVE_DATA>  &IdealTraj, std::vector<SM_CURVE_DATA>  &ApproxTraj, std::vector<double>& error, double *errMax_pos, int ind){
    error.clear();
    double err;
    for (int i = 0; i< ind; i++){
        err = sqrt(
                (IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])*(IdealTraj[i].Pos[0]-ApproxTraj[i].Pos[0])+
                (IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])*(IdealTraj[i].Pos[1]-ApproxTraj[i].Pos[1])+
                (IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2])*(IdealTraj[i].Pos[2]-ApproxTraj[i].Pos[2]));

        if(err > *errMax_pos) {
            *errMax_pos = err;
        }
    error.push_back(err);

    }
    return SM_OK;
}


SM_STATUS calcul_courbure(std::vector<SM_CURVE_DATA> &traj, std::vector<double> &curvature){

  unsigned int seuil = 65500;
  double curvature_tempo = 0.0;
  double prem_deriv_tempo = 0.0;
  double deux_deriv_tempo = 0.0;
  std::vector<double> prem_deriv_courbe;
  std::vector<double> deux_deriv_courbe;

//   prem_deriv_courbe.resize(traj.size());
//   deux_deriv_courbe.resize(traj.size());

  for (unsigned int i = 0; i < (traj.size() - 1); i++){
    if (traj[i+1].Pos[0] != traj[i].Pos[0]){
      prem_deriv_tempo = (traj[i+1].Pos[1] - traj[i].Pos[1]) / (traj[i+1].Pos[0] - traj[i].Pos[0]);
      prem_deriv_courbe.push_back(prem_deriv_tempo);
    }
    else{
      prem_deriv_tempo = seuil;
      prem_deriv_courbe.push_back(prem_deriv_tempo); // pour traiter le cas vertical
    }
  }
  prem_deriv_courbe.push_back(prem_deriv_tempo); // ajouter le dernier deriv : meme que l'avant dernier

  for (unsigned int i = 0; i < (traj.size() - 1); i++){
    if (traj[i+1].Pos[0] != traj[i].Pos[0]){
      deux_deriv_tempo = (prem_deriv_courbe.at(i+1) - prem_deriv_courbe.at(i)) / (traj[i+1].Pos[0] - traj[i].Pos[0]);
      deux_deriv_courbe.push_back(deux_deriv_tempo);
    }
    else{
      deux_deriv_tempo = seuil;
      deux_deriv_courbe.push_back(deux_deriv_tempo); // pour traiter le cas vertical
    }
  }
  deux_deriv_courbe.push_back(deux_deriv_tempo); // ajouter le dernier deriv : meme que l'avant dernier

  for (unsigned int i = 0; i < traj.size(); i++){
    curvature_tempo = (fabs(deux_deriv_courbe.at(i))) /
                      pow((1 + prem_deriv_courbe.at(i)*prem_deriv_courbe.at(i)), 1.5);
    curvature.push_back(curvature_tempo);
  }


  return SM_OK;
}

/*
SM_STATUS calcul_courbure(std::list<Path> &path, std::vector<double> &curvature, double tic){
    double t = 0.0;
    double sample_time = 0.001;
    int i = 0;
    int j = 0;
    Point2D dot;
    Point2D previous_dot;
    int nb_boucle = 1/sample_time;
    std::vector<double> tangente_pre;
    std::vector<double> tangente_sec;
    std::vector<double> x_diff;
    std::vector<double> y_diff;
  //  std::vector<double> curvature;

    std::list<SubPath>::iterator iter;

    for(iter=path.back().subpath.begin(); iter != path.back().subpath.end(); iter++) {

            if(iter->type == LINE) {
            double x_diff = (iter->end.x - iter->start.x);
            double y_diff = (iter->end.y - iter->start.y);
            sub_length = sqrt((iter->end.x - iter->start.x)*(iter->end.x - iter->start.x) + (iter->end.y - iter->start.y)*(iter->end.y - iter->start.y));

             for (t = 0.0; t<=1; t = t+sample_time){
                 vel_path_x.push_back(x_diff/sub_length);
                 vel_path_y.push_back(y_diff/sub_length);
             }

            TotalLength = sub_length + TotalLength;
            Lac[i] = TotalLength;
        }


         if (iter->type == BEZIER3){

            for (t = 0.0; t < 1; t = t+sample_time) {
                dot = bezier_point (t, iter->start, iter->bezier3[0], iter->bezier3[1], iter->end);

                if ((t > 0) || (iter!=path.back().subpath.begin())) {

                    x_diff.push_back ( dot.x - previous_dot.x );
                    y_diff.push_back ( dot.y - previous_dot.y );

                    if (x_diff[j-1] != 0)   {tangente_pre.push_back(y_diff[j-1]/x_diff[j-1]);}
                    else                    {tangente_pre[j-1] = tangente_pre[j-2];}

                    if ((t > sample_time) || (iter!=path.back().subpath.begin())){
                        tangente_sec.push_back ( (tangente_pre[j-1] - tangente_pre[j-2])/x_diff[j-2] );
                        curvature.push_back((abs(tangente_sec[j-2]))/pow((1+tangente_pre[j-2]*tangente_pre[j-2]),1.5));
                       // curvature_rad.push_back(1/curvature[j-2]);
                    }
                }
                j++;
                previous_dot = dot;
            }
           // sum = sum + curvature[]
        }
    }

    double seuil = 1000;


    for (int p = 0; p<curvature.size();p++){
        if (abs(curvature[p]) < seuil){
            curvature_filtre.push_back(curvature[p]);
        }else cout << curvature[p]<<endl;
    }
cout<<"break point"<<endl;

cout<<"break point"<<endl;
    return SM_OK;
}
*/



int sm_ComputeSmoothedStepVel(double vel, double tic, SM_LIMITS limitsGoto, SM_COND* cond) {
  
  double dc = 0.0;
  SM_COND FC;
  SM_STATUS resp;

  SM_TIMES T_Jerk;
  int dir = 0;
  double timeTrajDuration = 0.0;
  std::vector<double> I(3);
  std::vector<double> T(SM_NB_SEG);
  std::vector<double> J(SM_NB_SEG);
  std::vector<double> t(1);
  std::vector<double> a(1);
  std::vector<double> v(1);
  std::vector<double> x(1);

  FC.a = 0.0;
  FC.v = vel;
  FC.x = 1.0;

  cond->x = 0.0;

  if(ABS(FC.v - cond->v) > 0.001) {
    sm_CalculOfCriticalLength(*cond, FC, limitsGoto, &dc);
    if (isnan(dc)) {
      printf("isnan dc\n");
      printf("IC.a = %f    IC.v = %f    IC.x = %f\n", cond->a, cond->v, cond->x);
      printf("FC.a = %f    FC.v = %f    FC.x = %f\n",FC.a, FC.v, FC.x);
    }
	   
    FC.x = dc;
	    	    
    /* compute the motion */
    resp = sm_ComputeSoftMotion(*cond, FC, limitsGoto, &T_Jerk, &dir);
    if (resp != SM_OK) {
      printf("ERROR sm_ComputeSoftMotion\n");
    }

    /* get the  position at the next tick */       
    I[0] = cond->a;
    I[1] = cond->v;
    I[2] = cond->x;
      
    T[0] = T_Jerk.Tjpa;
    T[1] = T_Jerk.Taca;
    T[2] = T_Jerk.Tjna;
    T[3] = T_Jerk.Tvc;
    T[4] = T_Jerk.Tjnb;
    T[5] = T_Jerk.Tacb;
    T[6] = T_Jerk.Tjpb;
      
    J[0] =   dir*limitsGoto.maxJerk;
    J[1] =   0.0;
    J[2] = - dir*limitsGoto.maxJerk;
    J[3] =   0.0;
    J[4] = - dir*limitsGoto.maxJerk;
    J[5] =   0.0;
    J[6] =   dir*limitsGoto.maxJerk;

    timeTrajDuration = 0.0;
    for (int p=0; p < SM_NB_SEG; p++) {
      timeTrajDuration += T[p];
    }

    if(timeTrajDuration < tic) {
      t[0] = (double)timeTrajDuration; 
    } else {
      t[0] = (double)tic; 
    }
    resp = sm_AVX_TimeVar(I, T, J, t, a, v, x);
    cond->x = x[0];
    cond->v  = v[0];
    cond->a  = a[0];
    //printf("cond->v %f \n",cond->v); 
  } else {
    cond->v = vel;
  }
  return 0;
}

/* Return 0: OK
 * return 1: error
*/
int sm_ConvertPTPSM_MOTIONtoSM_TRAJ( SM_MOTION_MONO motion[], int nbJoints, SM_TRAJ &traj) {
  SM_STATUS resp;
  SM_COND IC[nbJoints][SM_NB_SEG];
  double  Time[nbJoints][SM_NB_SEG];
  double  Jerk[nbJoints][SM_NB_SEG];
  SM_SEG seg;
  std::vector<SM_SEG> traj_i;
  std::vector<double> I(3);
  std::vector<double> T(SM_NB_SEG);
  std::vector<double> J(SM_NB_SEG);
  std::vector<double> t(1);
  std::vector<double> a(1);
  std::vector<double> v(1);
  std::vector<double> x(1);

  for (int i = 0; i < nbJoints; i++)
    {

      I[0] = motion[i].IC.a;
      I[1] = motion[i].IC.v;
      I[2] = motion[i].IC.x;

      T[0] = motion[i].Times.Tjpa;
      T[1] = motion[i].Times.Taca;
      T[2] = motion[i].Times.Tjna;
      T[3] = motion[i].Times.Tvc;
      T[4] = motion[i].Times.Tjnb;
      T[5] = motion[i].Times.Tacb;
      T[6] = motion[i].Times.Tjpb;

      J[0] =   motion[i].Dir*motion[i].jerk.J1;
      J[1] =   0.0;
      J[2] = - motion[i].Dir*motion[i].jerk.J1;
      J[3] =   0.0;
      J[4] = - motion[i].Dir*motion[i].jerk.J1;
      J[5] =   0.0;
      J[6] =   motion[i].Dir*motion[i].jerk.J1;

      Time[i][0] = T[0];
      Time[i][1] = T[1];
      Time[i][2] = T[2];
      Time[i][3] = T[3];
      Time[i][4] = T[4];
      Time[i][5] = T[5];
      Time[i][6] = T[6];

      Jerk[i][0] = J[0];
      Jerk[i][1] = J[1];
      Jerk[i][2] = J[2];
      Jerk[i][3] = J[3];
      Jerk[i][4] = J[4];
      Jerk[i][5] = J[5];
      Jerk[i][6] = J[6];

      IC[i][0].a = I[0];
      IC[i][0].v = I[1];
      IC[i][0].x = I[2];
      t[0] = 0.0;
      for (int smp = 0; smp < SM_NB_SEG ; smp++) {
	resp = sm_AVX_TimeVar(I, T, J, t, a, v, x);
	IC[i][smp].a = a[0];
	IC[i][smp].v = v[0];
	IC[i][smp].x = x[0];     
	if (resp != SM_OK) {
	  printf("ERROR: Q interpolation failed (sm_AVX_TimeVar funcion)\n");
	  return 1;
	}
	t[0] += Time[i][smp];
      }
    }

  traj.clear();
  for (int i = 0; i < nbJoints; i++) {
    traj_i.clear();
    for (int j = 0; j < SM_NB_SEG; j++) {
      seg.IC = IC[i][j];
      seg.time = Time[i][j];
      seg.jerk = Jerk[i][j];
      traj_i.push_back(seg);     
    }
    traj.traj.push_back(traj_i);
    traj.qStart.push_back(motion[i].IC.x);
    traj.qGoal.push_back(motion[i].FC.x);
  }
  traj.computeTimeOnTraj();

  return 0;
}

