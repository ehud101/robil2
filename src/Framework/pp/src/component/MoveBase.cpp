/*
 * MoveBase.cpp
 *
 *  Created on: Mar 5, 2014
 *      Author: dan
 */

#include "MoveBase.h"
#include <Geometry.h>

#include <move_base_msgs/MoveBaseActionGoal.h>
#include "ComponentMain.h"

#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/PointCloud.h>

#include <tf_geometry/tf_geometry.h>


#define CREATE_MAP_FOR_NAV 0
#define CREATE_POINTCLOUD2_FOR_NAV 0
#define CREATE_POINTCLOUD_FOR_NAV 1

#define TH_NEARBY 1.5 //m

#include <opencv2/opencv.hpp>


#define SYNCH 	boost::recursive_mutex::scoped_lock locker(mtx);
#define REMOVE_Z(vA) vA.setZ(0)

namespace{

	long goal_counter =0;

	template<int N> double fround(double x){
		static const double k = ::pow10(N);
		return round(x*k)/k;
	}

	void remove_orientation(geometry_msgs::PoseStamped& goal){
		goal.pose.orientation.x = 0;
		goal.pose.orientation.y = 0;
		goal.pose.orientation.z = 0;
		goal.pose.orientation.w = 1;
	}
	double calcTriangle_deg(
			const geometry_msgs::Pose& A,
			const geometry_msgs::Pose& C,
			const geometry_msgs::Pose& B
	){
		btVector3 vA = toVector(A); REMOVE_Z(vA);
		btVector3 vB = toVector(B); REMOVE_Z(vB);
		btVector3 vC = toVector(C); REMOVE_Z(vC);
		double a = vC.distance(vB);//CB
		double c = vA.distance(vB);//AB
		double b = vA.distance(vC);//AC
		double gamma;// \_ ACB
		gamma = acos( (a*a + b*b - c*c)/(2*a*b) );
		return angles::to_degrees( angles::normalize_angle_positive( gamma ) );
	}
	size_t search_nearest_waypoint_index(const nav_msgs::Path& path, const geometry_msgs::PoseWithCovarianceStamped& pos){
		btVector3 vPose = toVector(getPose(pos)); REMOVE_Z(vPose);
		double min_idx=0;
		btVector3 path_poses_0 = toVector(getPose(path.poses[0])); REMOVE_Z(path_poses_0);
		double min_distance=path_poses_0.distance(vPose);
		for(size_t i=0;i<path.poses.size();i++){
			btVector3 path_poses_i = toVector(getPose(path.poses[i])); REMOVE_Z(path_poses_i);
			double c_distance=path_poses_i.distance(vPose);
			if(c_distance<min_distance){
				min_distance = c_distance;
				min_idx = i;
			}
		}
		return min_idx;
	}

	geometry_msgs::PoseStamped search_next_waypoint(const nav_msgs::Path& path, const geometry_msgs::PoseWithCovarianceStamped& pos, bool& path_is_finished, int& goal_res_index){
		//cout<<"search_next_waypoint for: "<<pos.pose.pose.position.x<<","<<pos.pose.pose.position.y<<endl;
		if(path.poses.size()==0){
			path_is_finished = true;
			goal_res_index = -1;
			return getPoseStamped( pos );
		}
		if(path.poses.size()==1){
			geometry_msgs::PoseStamped my_pose = getPoseStamped( pos );
			geometry_msgs::PoseStamped path_pose = getPoseStamped( path.poses[0] );
			btVector3 vpath_pose_pose = toVector(path_pose.pose); REMOVE_Z(vpath_pose_pose);
			btVector3 vmy_pose_pose = toVector(my_pose.pose); REMOVE_Z(vmy_pose_pose);
			path_is_finished = vpath_pose_pose.distance(vmy_pose_pose) <= TH_NEARBY;
			goal_res_index = 0;
			return path_pose;
		}
		path_is_finished = false;
		size_t ni = search_nearest_waypoint_index(path , pos);
		//cout<<"[i] nearest index = "<<ni<<endl;
		const geometry_msgs::Pose& c = getPose( pos );
		//FIRST POINT
		if(ni==0){
			const geometry_msgs::Pose& b = getPose( path.poses[0] );
			const geometry_msgs::Pose& d = getPose( path.poses[1] );
			double angle_deg = calcTriangle_deg(d,b,c);
			bool angle_ok = angle_deg<=90;
			bool distance_ok = (toVector(b)-toVector(c)).length() <= TH_NEARBY;
			if(not (angle_ok or distance_ok) ){
				//cout<<"[i] return index = 0"<<endl;
				tf_geometry::Position p1(path.poses[0].pose.position);
				tf_geometry::Position p2(path.poses[1].pose.position);
				tf_geometry::Position res = p1+((p2-p1).normalized()*0.5);
				geometry_msgs::PoseStamped pose_res=path.poses[0];
				pose_res.pose.position = res.to_msg_Point();
				goal_res_index = 0;
				return getPoseStamped( pose_res );
//				return getPoseStamped( path.poses[0] );
			}
			ni+=1;
		}
		//OTHERS POINTS
		for(size_t i=ni;i<path.poses.size();i++){
			const geometry_msgs::Pose& a = getPose( path.poses[i-1] );
			const geometry_msgs::Pose& b = getPose( path.poses[i] );
			double angle_deg = calcTriangle_deg(a,b,c);
			bool angle_ok = angle_deg>=90;
			bool distance_ok = (toVector(b)-toVector(c)).length() <= TH_NEARBY;
			if(not (angle_ok or distance_ok) ){
				//cout<<"[i] return index = "<< i <<endl;
				if(i==path.poses.size()-1){
					geometry_msgs::PoseStamped my_pose = getPoseStamped( pos );
					geometry_msgs::PoseStamped path_pose = getPoseStamped( path.poses[i] );
					path_is_finished = toVector(path_pose.pose).distance(toVector(my_pose.pose)) <= TH_NEARBY;
				}
				if(i==path.poses.size()-1){
					goal_res_index = i;
					return getPoseStamped( path.poses[i] );
				}
				tf_geometry::Position p1(path.poses[i-1].pose.position);
				tf_geometry::Position p2(path.poses[i].pose.position);
				tf_geometry::Position res = p2+((p2-p1).normalized()*0.5);
				geometry_msgs::PoseStamped pose_res=path.poses[i];
				pose_res.pose.position = res.to_msg_Point();
				goal_res_index = i;
				return getPoseStamped( pose_res );
			}
		}
		//PATH IS FINISHED
		path_is_finished = true;
		goal_res_index = -2;
		return getPoseStamped( pos );
		//return getPoseStamped( path.poses[path.poses.size()-1] );
	}


	struct point_t{
		int x,y;
		point_t(int x,int y):x(x),y(y){}
		int index(int w,int h)const{return y*w+x;}
		bool inside(int w,int h)const{return 0<=x and x<w and 0<=y and y<h;}
	};
	bool make_goal_reachable(
		geometry_msgs::PoseStamped& goal, int& goal_index,
		const nav_msgs::Path& path,
		const geometry_msgs::PoseWithCovarianceStamped& pos,
		const nav_msgs::OccupancyGrid& global_map,
		bool& path_is_finished
	)
	{
#		define SHOW_CV_RESULTS 0
		bool result(false);
		int data_size =global_map.data.size();
		int w = global_map.info.width;
		int h = global_map.info.height;
		bool* reachable = new bool[data_size]; memset(reachable,0,data_size*sizeof(bool));
		std::list<point_t> next;
		point_t center(w/2,h/2);
		try{
			reachable[center.index(w,h)] = true;																					// select all reachable cells
			next.push_back(center);
			while(next.empty()==false){
				point_t current = next.front(); next.pop_front();
				for(int iy=-1;iy<=1;iy++)for(int ix=-1;ix<=1;ix++){
					point_t nei(current.x+ix,current.y+iy);
					if/*outside of map*/	(nei.inside(w,h)==false) 				continue;
					if/*already checked*/	(reachable[nei.index(w,h)]) 			continue;
					if/*occupied*/			(global_map.data[nei.index(w,h)]>10) 	continue;
					/*else*/
					reachable[nei.index(w,h)] = true;
					next.push_back(nei);
				}
			}
			for(int y=0;y<h;y++)for(int x=0;x<w;x++){
				point_t current = point_t(x,y);
				bool v = global_map.data[current.index(w,h)]>10;
				if(v){
					for(int iy=-1;iy<=1;iy++)for(int ix=-1;ix<=1;ix++){
						point_t nei(current.x+ix,current.y+iy);
						if/*outside of map*/	(nei.inside(w,h)==false) 			continue;
						reachable[nei.index(w,h)] = false;
					}
				}
			}
#			if SHOW_CV_RESULTS==1
				cv::Mat show(h,w, CV_8UC3);
				for(int y=0;y<h;y++)for(int x=0;x<w;x++){
					uchar v = reachable[point_t(x,y).index(w,h)] ? 255 : 0;
					show.at<cv::Vec3b>(h-y-1,x) = cv::Vec3b(v,v,v);
				}
				cv::namedWindow("REACHABLE", CV_WINDOW_NORMAL);
#			endif
			point_t _goal(goal.pose.position.x/global_map.info.resolution, goal.pose.position.y/global_map.info.resolution);
			point_t _robot(pos.pose.pose.position.x/global_map.info.resolution, pos.pose.pose.position.y/global_map.info.resolution);
			point_t goal_cell(_goal.x-_robot.x+center.x, _goal.y-_robot.y+center.y);													// calculate cell of current goal
			bool is_reachable = goal_cell.inside(w,h) and reachable[goal_cell.index(w,h)];
			if( not is_reachable ){																									// if the cell is not reachable then
				point_t best(0,0);																									//		search nearest to the cell reachable point, change goal and return true
				double best_dist=hypot(w,h);
				for(int y=0;y<h;y++)for(int x=0;x<w;x++){
					point_t p(x,y);
					if( not reachable[p.index(w,h)] ) continue;
					double dist = hypot(p.x-goal_cell.x, p.y-goal_cell.y);
					if(best_dist > dist){ best_dist=dist; best=p; }
				}
				goal.pose.position.x = (best.x-center.x) * global_map.info.resolution + pos.pose.pose.position.x;
				goal.pose.position.y = (best.y-center.y) * global_map.info.resolution + pos.pose.pose.position.y;
#				if SHOW_CV_RESULTS==1
					show.at<cv::Vec3b>(h-goal_cell.y-1,goal_cell.x) = cv::Vec3b(100,255,200);
					show.at<cv::Vec3b>(h-best.y-1,best.x) = cv::Vec3b(255,0,0);
					cv::imshow("REACHABLE",show);
					cv::waitKey(10);
#				endif
				result = true;
			}
			else { result = false; }																								// else return false

		}catch (...) {}
		delete[] reachable;
		return result;
	}

	boost::thread_group threads;
	actionlib_msgs::GoalID last_nav_goal_id;
	move_base_msgs::MoveBaseActionGoal last_nav_goal;
	nav_msgs::Path curr_nav_path;
	bool clear_path_on_activate=true;

	boost::mutex global_map_mutex;
	nav_msgs::OccupancyGrid global_cost_map;
}


void on_GlobalCostMap( const nav_msgs::OccupancyGrid::ConstPtr& cost_map){
	boost::mutex::scoped_lock l(global_map_mutex);
	global_cost_map = *cost_map;
	//ROS_INFO("Navigation: Global occupancy cost grid gotten");
}
void on_speed(const geometry_msgs::Twist::ConstPtr& msg){
	bool moving = msg->linear.x + msg->linear.y + msg->angular.z;
	if(not moving){
		//ROS_INFO_STREAM( "Navigation: ROBOTE IS STOPPED" );
	}
}


MoveBase::MoveBase(ComponentMain* comp)
	:is_active(true), gp_defined(false),gnp_defined(false), gl_defined(false), comp(comp), is_canceled(true), is_path_calculated(false)
{
	ros::NodeHandle node;

	goalPublisher = node.advertise<move_base_msgs::MoveBaseActionGoal>("/move_base/goal", 5, false);
	originalGoalPublisher = node.advertise<geometry_msgs::PoseStamped>("/move_base/original_goal", 5, false);
	goalCancelPublisher = node.advertise<actionlib_msgs::GoalID>("/move_base/cancel", 5, false);
	pathSubscriber = node.subscribe("/move_base/NavfnROS/plan", 10, &MoveBase::on_nav_path, this);
	globalCostmapSubscriber = node.subscribe("/move_base/global_costmap/costmap", 1, &on_GlobalCostMap);
	speedSubscriber = node.subscribe("/cmd_vel", 1, &on_speed);
	globalPathPublisher = node.advertise<nav_msgs::Path>("/pp/global_path",1);
	selectedPathPublisher = node.advertise<nav_msgs::Path>("/pp/selected_path",1);
	//moveBaseStatusSubscriber = node.subscribe("/move_base/status", 1, &MoveBase::on_move_base_status, this);

#if CREATE_POINTCLOUD_FOR_NAV == 1
	mapPublisher = node.advertise<sensor_msgs::PointCloud>("/map_cloud", 5, false);
	fakeLaserPublisher = node.advertise<sensor_msgs::LaserScan>("/costmap_clear_fake_scan",1,false);
#endif

	sub_log = node.subscribe("/rosout", 10, &MoveBase::on_log_message, this);

//	//FOR TEST
	sub_location = node.subscribe("/test/location", 10, &MoveBase::on_sub_loc, this);
	sub_location_cov = node.subscribe("/test/location_cov", 10, &MoveBase::on_sub_loc_cov, this);
	sub_map = node.subscribe("/test/map", 10, &MoveBase::on_sub_map, this);
	sub_path = node.subscribe("/test/path", 10, &MoveBase::on_sub_path, this);
	sub_commands = node.subscribe("/test/command", 10, &MoveBase::on_sub_commands, this);
}

void MoveBase::on_move_base_status(const actionlib_msgs::GoalStatusArray::ConstPtr& msg){
	if( not is_active ) return;
	size_t n = msg->status_list.size();
	for(size_t i=0;i<n;i++){
		const actionlib_msgs::GoalStatus& status = msg->status_list[i];
		if( status.text == "" or status.text == "''" ){
			//ROS_WARN_STREAM("Navigation: move_base status is empty => aborted?");
			//on_error_from_move_base();
		}
	}
}

void MoveBase::on_log_message(const LogMessage::ConstPtr& msg){
	if(msg->name == "/move_base" and msg->level > LogMessage::DEBUG){
		on_log_message(msg->level, msg->msg);
	}
}
void MoveBase::on_log_message(int type, string message){
	if(type == LogMessage::WARN){
		ROS_DEBUG_STREAM("move base: warning: "<<message);
	}else
	if(type == LogMessage::ERROR){
		ROS_DEBUG_STREAM("move base: error: "<<message);
		on_error_from_move_base();
	}else
	if(type == LogMessage::FATAL){
		ROS_DEBUG_STREAM("move base: fatal: "<<message);
		on_error_from_move_base();
	}
}

void MoveBase::on_error_from_move_base(){
	ROS_ERROR("Navigation: path is aborted. send event and clear current path.");
	stop_navigation(false);
}
void MoveBase::stop_navigation(bool success){
	notify_path_is_finished(success);
	gp_defined=gnp_defined=false;
	this->is_path_calculated = false;
	goalCancelPublisher.publish(last_nav_goal_id);
}

//================ TEST =================
void MoveBase::on_sub_map(const nav_msgs::OccupancyGrid::ConstPtr& msg){
	this->on_map(*msg);
}
void MoveBase::on_sub_path(const nav_msgs::Path::ConstPtr& msg){
	this->on_path(*msg);
}
void MoveBase::on_sub_loc(const geometry_msgs::PoseStamped::ConstPtr& msg){
	geometry_msgs::PoseWithCovarianceStamped p;
	p.header.frame_id = "/map";//NOTE: check if frame_id not has to be from robil_map.header
	p.header.stamp = ros::Time::now();
	p.pose.pose = msg->pose;
	this->on_position_update(p);
}
void MoveBase::on_sub_loc_cov(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& msg){
	//std::cout<<"path.poses.push_back(createPose("<< msg->pose.pose.position.x<<","<< msg->pose.pose.position.y<<"));"<<std::endl;
	geometry_msgs::PoseWithCovarianceStamped p = *msg;
	p.header.frame_id = "/map";//NOTE: check if frame_id not has to be from robil_map.header
	this->on_position_update(p);
}
void MoveBase::on_sub_commands(const std_msgs::String::ConstPtr& msg){
	SYNCH
	std::string command = msg->data;
	if(command == "clear"){
		ROS_INFO_STREAM("Navigation: on command CLEAR");
		this->gl_defined = false;
		this->gnp_defined= false;
		this->gp_defined = false;
	}
	if(command == "cancel"){
		ROS_INFO_STREAM("Navigation: on command CANCEL");
		this->cancel();
	}
}
//=======================================

MoveBase::~MoveBase() {

}

bool MoveBase::all_data_defined()const{
	return gl_defined and (gp_defined or gnp_defined) and is_active and not is_canceled;
}

void MoveBase::notify_path_is_finished(bool success)const{
	if(success) comp->rise_taskFinished();
	else comp->rise_taskAborted();
}


void MoveBase::on_position_update(const config::PP::sub::Location& location){
SYNCH
	//ROS_INFO_STREAM("\t on_position_update( "<<location.pose.pose.position.x<<","<<location.pose.pose.position.y<<" )");
	gotten_location = location;
	gl_defined=true;
	if(all_data_defined()) calculate_goal();
}

#define STR(P) P.x<<","<<P.y

void MoveBase::on_path(const config::PP::sub::GlobalPath& goal_path){
SYNCH

	ROS_INFO_STREAM("Navigation: Global path gotten. Number of way points is "<<goal_path.waypoints.poses.size()<<" ");
	if(goal_path.waypoints.poses.size()==0) return;
	gotten_path = goal_path;
	gp_defined=true;

	if(not is_active){
		ROS_WARN_STREAM("New Global Path is rejected, because navigation is deactivated");
		return;
	}
	if(all_data_defined()) calculate_goal();
}
void MoveBase::on_path(const nav_msgs::Path& goal_path){
SYNCH

	ROS_INFO_STREAM("Navigation: Global path gotten. Number of way points is "<<goal_path.poses.size()<<" ");
	if(goal_path.poses.size()==0) return;
	gotten_nav_path = goal_path;
	gnp_defined=true;

	if(not is_active){
		ROS_WARN_STREAM("New Global Path is rejected, because navigation is deactivated");
		return;
	}
	if(all_data_defined()) calculate_goal();
}


void MoveBase::cancel(bool clear_last_goals){
	SYNCH
	is_canceled = true;
	this->gl_defined = false;
	if(clear_last_goals){
		this->gnp_defined= false;
		this->gp_defined = false;
	}
	this->is_path_calculated = false;
	last_nav_goal = move_base_msgs::MoveBaseActionGoal();
	goalCancelPublisher.publish(last_nav_goal_id);
	goal_counter++;
}
void MoveBase::activate(){
	SYNCH
	is_active=true;
	is_canceled=false;
	last_nav_goal = move_base_msgs::MoveBaseActionGoal();
	ROS_INFO("Navigation is active. you can send Global Path.");
	if(clear_path_on_activate){
		this->gnp_defined= false;
		this->gp_defined = false;
		ROS_INFO("  the previous path is deleted");
	}else{
		ROS_INFO("  the previous path is restored.");
	}
}
void MoveBase::deactivate(bool clear_last_goals){
	SYNCH
	is_active=false;
	clear_path_on_activate = clear_last_goals;
	cancel();
	ROS_INFO("Navigation is deactivated. The driver is stopped and each new Global Path will rejected up to activation.");
}

void path_publishing(ComponentMain* comp, boost::recursive_mutex* mtx, bool* is_canceled, bool* is_path_calculated){
	double fr = 10; double time =1.0/fr*1000.0;
	while(not boost::this_thread::interruption_requested() and ros::ok()){
		boost::posix_time::ptime t = boost::get_system_time();
		{
			boost::recursive_mutex::scoped_lock locker(*mtx);
			if(*is_canceled or not *is_path_calculated) continue;

			config::PP::pub::LocalPath lpath;
			lpath.is_heading_defined=false;
			lpath.is_ip_defined=false;
			lpath.waypoints = curr_nav_path;

			comp->publishLocalPath(lpath);
		}
		boost::this_thread::sleep(t+boost::posix_time::millisec(time));
	}
}

void MoveBase::on_nav_path(const nav_msgs::Path& nav_path){
	SYNCH
	if(not is_active) return;

	is_path_calculated = true;
	curr_nav_path = nav_path;
	if(threads.size()==0) threads.add_thread(new boost::thread(boost::bind(path_publishing, comp, &mtx, &is_canceled, &is_path_calculated)));
}



void MoveBase::calculate_goal(){
	geometry_msgs::PoseStamped goal;
	bool is_path_finished;
	int goal_index=-1;
	nav_msgs::Path gotten_global_path;
	if(gp_defined){
		gotten_global_path = gotten_path.waypoints;
	}else{
		gotten_global_path = gotten_nav_path;
	}

	gotten_global_path.header.frame_id = "/map";
	gotten_global_path.header.stamp = ros::Time(0) ;
	globalPathPublisher.publish(gotten_global_path);
	selectedPathPublisher.publish(gotten_global_path);

	goal = search_next_waypoint(gotten_global_path, gotten_location, is_path_finished, goal_index);

	if(is_path_finished){
		ROS_INFO("Navigation: path is finished. send event and clear current path.");
		stop_navigation(true);
		return;
	}

	global_map_mutex.lock();
		nav_msgs::OccupancyGrid gmap = global_cost_map;
	global_map_mutex.unlock();
	if( gmap.info.width and gmap.info.height ){
		geometry_msgs::PoseStamped original_goal = goal;
		geometry_msgs::PoseStamped first_original_goal = goal;
		static bool prev_send_original(true);
		bool send_original = false;
		//ROS_INFO_STREAM("Navigation: check goal on reachability ...");
		while(
				not is_path_finished
				and
				make_goal_reachable(goal, goal_index, gotten_global_path, gotten_location, gmap, is_path_finished)
		){
			send_original = true;
			//ROS_INFO_STREAM("Navigation: ... original goal("<<original_goal.pose.position.x<<","<<original_goal.pose.position.y<<") is not reachable. it's changed to near one("<<goal.pose.position.x<<","<<goal.pose.position.y<<").");
			gotten_global_path.poses[goal_index] = goal;
			selectedPathPublisher.publish(gotten_global_path);

			goal = search_next_waypoint(gotten_global_path, gotten_location, is_path_finished, goal_index);
			if(goal.pose.position.x==original_goal.pose.position.x and goal.pose.position.y==original_goal.pose.position.y) break;
			original_goal = goal;
		}
		if(send_original){
			first_original_goal.header.frame_id = "/map";
			first_original_goal.header.stamp = ros::Time::now();
			originalGoalPublisher.publish(first_original_goal);
			if(send_original!=prev_send_original){
				ROS_INFO_STREAM("Navigation: Goal changed from original to reachable");
			}
		}else{
			if(send_original!=prev_send_original){
				ROS_INFO_STREAM("Navigation: Original goal is used");
			}
		}
		prev_send_original=send_original;
		//ROS_INFO_STREAM("Navigation: ... done");
	}else{
		ROS_WARN("Navigation: Global occupancy cost map is not defined");
	}

	if(is_path_finished){
		ROS_INFO("Navigation: path is finished (case2). send event and clear current path.");
		stop_navigation(true);
		return;
	}

	on_goal(goal);
}

void MoveBase::on_goal(const geometry_msgs::PoseStamped& robil_goal){
	double robil_goal_x = fround<1>(robil_goal.pose.position.x);
	double robil_goal_y = fround<1>(robil_goal.pose.position.y);
	if(
		last_nav_goal.goal.target_pose.pose.position.x == robil_goal_x and
		last_nav_goal.goal.target_pose.pose.position.y == robil_goal_y
	){
		//ROS_INFO_STREAM("Navigation: goal is rejected. The same one.");
		return;
	}

	move_base_msgs::MoveBaseActionGoal goal;
	geometry_msgs::PoseStamped ps_goal;

	ps_goal = robil_goal;
	remove_orientation(ps_goal);
	ps_goal.pose.position.x = robil_goal_x;
	ps_goal.pose.position.y = robil_goal_y;

	goal.header.frame_id = "/map";
	goal.header.stamp = ros::Time::now();
	ps_goal.header = goal.header;

	std::stringstream sid ;
	sid<<"[i] goal #"<< boost::lexical_cast<std::string>(goal_counter) <<": "
					 << ps_goal.pose.position.x<<","<<ps_goal.pose.position.y;
	goal.goal_id.id = sid.str();
	goal.goal_id.stamp = goal.header.stamp;
	
	goal.goal.target_pose = ps_goal;
	
	//ROS_INFO_STREAM("Navigation: set new goal : "<<ps_goal.pose.position.x<<","<<ps_goal.pose.position.y);
	last_nav_goal_id = goal.goal_id;
	last_nav_goal = goal;
	goalPublisher.publish(goal);


}

void MoveBase::on_map(const nav_msgs::OccupancyGrid& nav_map){
SYNCH
	nav_msgs::OccupancyGrid map = nav_map;
	mapPublisher.publish(map);
}


void MoveBase::on_map(const config::PP::sub::Map& robil_map){
SYNCH

#if CREATE_POINTCLOUD_FOR_NAV == 1

	sensor_msgs::PointCloud map;
	static ros::NodeHandle node("~");
	int rad_clear =2;
	if(not node.getParamCached("clear_map_radius",rad_clear) ) node.setParam("clear_map_radius",rad_clear);

	double x_offset = robil_map.info.width*robil_map.info.resolution * 2.0/3.0;
	double y_offset = robil_map.info.height*robil_map.info.resolution * 1.0/2.0;

	for(
			size_t y=0;
			y<robil_map.info.height;
			y++
	){
		float fy = y*robil_map.info.resolution;
		for(
				size_t x=0;
				x<robil_map.info.width;
				x++
		){
			size_t i = y*robil_map.info.width+x;
			float fx = x*robil_map.info.resolution;


			float fz = 0;

			if(robil_map.data[i].type==robil_msgs::MapCell::type_obstacle /*or rand()%10>8*/){
				fz = 1 - gotten_location.pose.pose.position.z;
			}else
			if(robil_map.data[i].type==robil_msgs::MapCell::type_clear){
				continue;
				fz = 0;
			}else
			if(robil_map.data[i].type==robil_msgs::MapCell::type_unscanned){
				continue;
				fz = -1;
			}
			geometry_msgs::Point32 point;
			point.x = -fx + x_offset;
			point.y = -fy + y_offset;
			point.z = fz;

			if( hypot( point.x, point.y ) < rad_clear ) continue;
			map.points.push_back(point);
		}
	}

	map.header.frame_id = "/base_link";
	map.header.stamp = ros::Time::now();//robil_map.header.stamp;//


	static sensor_msgs::LaserScan fake_scan; static bool fsd=false; static long fk_iter=0;fk_iter++;
	fake_scan.header = map.header;
	if(!fsd){fsd=true;
		const int ranges_count=359*1;
		fake_scan.angle_min = 0;
		fake_scan.angle_max = (359.0/360.0)*2*M_PI;
		fake_scan.angle_increment = fake_scan.angle_max/ranges_count;
		fake_scan.range_min=0.1;
		fake_scan.range_max=31;
		fake_scan.ranges.resize(ranges_count);
		for(int i=0;i<ranges_count;i++) fake_scan.ranges[i]=30;
	}
	if(fk_iter%10==0) fakeLaserPublisher.publish(fake_scan);
	mapPublisher.publish(map);

#endif


}
