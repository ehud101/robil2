/*
 * ComponentMain.h
 *
 *  Created on: Jan 16, 2014
 *      Author: yuval
 */

#ifndef COMPONENTMAIN_H_
#define COMPONENTMAIN_H_

#include <std_msgs/String.h>
#include <ParameterTypes.h>

class RosComm;

class ComponentMain {
	RosComm* _roscomm;
public:
	ComponentMain(int argc,char** argv);
	virtual ~ComponentMain();

        void handleMap(const config::PP::sub::Map& msg);
        void handleMissionGlobalPath(const config::PP::sub::MissionGlobalPath& msg);
        void handleIEDPosAtt(const config::PP::sub::IEDPosAtt& msg);
        void handlePosAttVel(const config::PP::sub::PosAttVel& msg);
        void handleRPPPath(const config::PP::sub::RPPPath& msg);

        void publishLocalPathPlan(config::PP::pub::LocalPathPlan& msg);
};

#endif /* COMPONENTMAIN_H_ */
