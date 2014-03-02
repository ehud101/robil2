
/*
 * ComponentMain.cpp
 *
 *  Created on: Thursday, 27. February 2014 12:29PM
 *      Author: autogenerated
 */
#include "ComponentMain.h"
#include "../roscomm/RosComm.h"
#include "MissionManager.h"
ComponentMain::ComponentMain(int argc,char** argv)
{
	_roscomm = new RosComm(this,argc, argv);
	_mission_manager = new MissionManager();
}
ComponentMain::~ComponentMain() {
	if(_roscomm) delete _roscomm; _roscomm=0;
	if(_mission_manager) delete _mission_manager; _mission_manager=0;
}

void ComponentMain::handleAssignNavTask(const config::SMME::sub::AssignNavTask& msg)
{
	std::cout<< "SMME say:" << msg << std::endl;
}
	

void ComponentMain::handleAssignManTask(const config::SMME::sub::AssignManTask& msg)
{
	std::cout<< "SMME say:" << msg << std::endl;
}
	

void ComponentMain::handleAssignMission(const config::SMME::sub::AssignMission& msg)
{
	std::cout<< "SMME say:" << msg << std::endl;
}
	

void ComponentMain::handleBladePosition(const config::SMME::sub::BladePosition& msg)
{
	std::cout<< "SMME say:" << msg << std::endl;
}
	

void ComponentMain::handleLocation(const config::SMME::sub::Location& msg)
{
	std::cout<< "SMME say:" << msg << std::endl;
}
	

void ComponentMain::publishGlobalPath(config::SMME::pub::GlobalPath& msg)
{
	_roscomm->publishGlobalPath(msg);
}
	

void ComponentMain::publishWorkSeqData(config::SMME::pub::WorkSeqData& msg)
{
	_roscomm->publishWorkSeqData(msg);
}
	

void ComponentMain::publishMissionAcceptance(config::SMME::pub::MissionAcceptance& msg)
{
	_roscomm->publishMissionAcceptance(msg);
}
	
void ComponentMain::publishTransform(const tf::Transform& _tf, std::string srcFrame, std::string distFrame){
	_roscomm->publishTransform(_tf, srcFrame, distFrame);
}
tf::StampedTransform ComponentMain::getLastTrasform(std::string srcFrame, std::string distFrame){
	return _roscomm->getLastTrasform(srcFrame, distFrame);
}
void ComponentMain::publishDiagnostic(const diagnostic_msgs::DiagnosticStatus& _report){
	_roscomm->publishDiagnostic(_report);
}
void ComponentMain::publishDiagnostic(const std_msgs::Header& header, const diagnostic_msgs::DiagnosticStatus& _report){
	_roscomm->publishDiagnostic(header, _report);
}
