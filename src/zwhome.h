/*
 * zwhome.h
 *
 *  Created on: Jun 21, 2015
 *      Author: joe
 */

#ifndef ZWHOME_H_
#define ZWHOME_H_

#include "Notification.h"
#include "zwPrefs.h"
#include "zwRules.h"
#include "zwDevices.h"
#include "zwUtils.h"
using namespace OpenZWave;
using namespace std;

void OnNotification(Notification const* _notification, void* _context);
extern zwPrefs *zwP;
extern zwRules *zwR;
extern zwDevices *zwD;
extern FILE *zl;

#endif /* ZWHOME_H_ */
