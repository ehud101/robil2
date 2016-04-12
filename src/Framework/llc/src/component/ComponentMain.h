
/*
 * ComponentMain.h
 *
 *  Created on: Thursday, 27. February 2014 12:29PM
 *      Author: autogenerated
 */
#ifndef COMPONENTMAIN_H_
#define COMPONENTMAIN_H_
#include <ros/ros.h>
#include <std_msgs/String.h>
#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>
#include <ParameterTypes.h>
#include <tf/tf.h>
#include <boost/thread.hpp>




class ComponentMain {
	bool _inited;

	  ros::NodeHandle _nh;
	  ros::Publisher _pub_diagnostic;
	  boost::thread_group _maintains;
		ros::Subscriber _sub_WPDVelocity;
		ros::Subscriber _sub_WSMVelocity;
		ros::Subscriber _sub_BladePositionCommand;
		ros::Subscriber _sub_Location;
		ros::Subscriber _sub_PerVelocity;
		ros::Publisher  _pub_EffortsTh;
		ros::Publisher  _pub_EffortsSt;
		ros::Publisher  _pub_EffortsJn;
		ros::Publisher  _pub_Speed;

	  bool init(int argc,char** argv);
public:
	//config::LLC::sub::PerVelocity Per_measured_speed ;	/* real measured speed */
	geometry_msgs::Twist Per_measured_speed ;
	config::LLC::sub::WSMVelocity WSM_desired_speed ;		/* WSM desired vel. - on work sequence mode */
	config::LLC::sub::WPDVelocity WPD_desired_speed ;		/* WPD desired vel. - on driving mode */
	config::LLC::sub::BladePositionCommand Blade_angle ; 	/* WSM desired supporter positon */
	config::LLC::sub::Location Per_pose ; 					/* Per location */
	int t_flag;

	ComponentMain(int argc,char** argv);
	virtual ~ComponentMain();
	void handleWPDVelocity(const config::LLC::sub::WPDVelocity& msg);
	void handleWSMVelocity(const config::LLC::sub::WSMVelocity& msg);
	void handleBladePositionCommand(const config::LLC::sub::BladePositionCommand& msg);
	void handleLocation(const config::LLC::sub::Location& msg);
	void handlePerVelocity(const config::LLC::sub::PerVelocity& msg);
	void publishEffortsTh(config::LLC::pub::EffortsTh& msg);
	void publishEffortsSt(config::LLC::pub::EffortsSt& msg);
	void publishEffortsJn(sensor_msgs::JointState& msg);
	void publishTransform(const tf::Transform& _tf, std::string srcFrame, std::string distFrame);
	tf::StampedTransform getLastTransform(std::string srcFrame, std::string distFrame);
	void publishDiagnostic(const diagnostic_msgs::DiagnosticStatus& _report);
	void publishDiagnostic(const std_msgs::Header& header, const diagnostic_msgs::DiagnosticStatus& _report);
	void heartbeat();
};
#endif /* COMPONENTMAIN_H_ */

