/*
 * ComponentMain.h
 *
 *  Created on: Jan 16, 2014
 *      Author: yuval
 */

#ifndef COMPONENTMAIN_H_
#define COMPONENTMAIN_H_

#include <std_msgs/String.h>

class RosComm;

class ComponentMain {
	RosComm *_roscomm;
public:
	ComponentMain(int argc,char** argv);
	virtual ~ComponentMain();

        void handleMap(std_msgs::String msg);
        void handleMissionGlobalPath(std_msgs::String msg);
        void handleIEDPosAtt(std_msgs::String msg);
        void handlePosAttVel(std_msgs::String msg);
        void handleRPPPath(std_msgs::String msg);

        void publishLocalPathPlan(std_msgs::String &msg);
};

#endif /* COMPONENTMAIN_H_ */
