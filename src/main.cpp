#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

// given the vehicle's position, find the cloest waypoint in the map
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

	//initial value, has to be outside the h.onMessage call
	int lane = 1;
	double ref_vel = 0; // in mph, 0 so we avoid cold start

  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy,&lane,&ref_vel](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

						// constants
						double max_vel = 49.5; //mph
						double max_acc = .224; //5m/s^2, under the 10m/s^2 requirement
						double time_inc = .02; // time, in second, it takes to go from one point in the path to the next
						double dist_inc = .44704;

						int previous_path_size = previous_path_x.size();

						if (previous_path_size > 0) {
							car_s = end_path_s;
						}

						// initialize for finite state machine
						bool too_close = false;
						bool can_turn_left = false;
						bool can_turn_right = false;
						int check_car_lane = 1;

						// track all vehicles in adjacent lanes
						vector <bool> left_turns;
						vector <bool> right_turns;

						// loop through all the sensor fusion data, i.e. all the other vehicles detected
						for(int i=0; i<sensor_fusion.size(); i++) {
							// check if this car is in our lane
							float d = sensor_fusion[i][6];

							if (d >= 0 && d < 4) {
								check_car_lane = 0; // left lane
							} else if (d >= 4 && d < 8) {
								check_car_lane = 1; // middle lane
							} else {
								check_car_lane = 2;
							}

							double vx = sensor_fusion[i][3];
							double vy = sensor_fusion[i][4];
							double check_car_speed = sqrt(vx*vx+vy*vy); // true speed of the detected vehicle
							double check_car_s = sensor_fusion[i][5];

							// predict where the car will be in the future, time_inc*check_car_speed gives us distance traveled
							// in each time increment of time_inc second, and there are previous_path_size number of increments,
							// so this product, previous_path_size*time_inc*check_car_speed, gives us where this detected car, check_car,
							// is going to be at the end of previous_path's trajectory
							check_car_s += (double)previous_path_size*time_inc*check_car_speed;
							int lane_diff = check_car_lane - lane;
							double s_dist_diff = abs(check_car_s - car_s);
							bool can_turn = s_dist_diff > 30;

							// behavioral logic
							if (lane_diff == 0) {
								// check if car is too close
								// if detected car is in front of us and is within 30 meters, reduce speed
								if (check_car_s > car_s && (s_dist_diff) < 30) {
									too_close = true;
								}
							} else if (lane_diff == -1) {
								// car is in the lane left of us, can turn left if it's at least 30m away in the s direction
								left_turns.push_back(can_turn);
							} else if (lane_diff == 1) {
								// car is in the lane right of us, can turn left if it's at least 30m away in the s direction
								right_turns.push_back(can_turn);
							}
						}

						// can turn if confirm no cars are in adjacent lanes
						can_turn_left = std::all_of(left_turns.begin(), left_turns.end(), [](bool can_turn) {return can_turn;});
						can_turn_right = std::all_of(right_turns.begin(), right_turns.end(), [](bool can_turn) {return can_turn;});

          	json msgJson;

          	vector<double> next_x_vals;
          	vector<double> next_y_vals;

          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds

						vector<double> ptsx;
						vector<double> ptsy;

						// reference points
						double ref_x = car_x;
						double ref_y = car_y;
						double ref_yaw = deg2rad(car_yaw);

						if (previous_path_size < 2) {
							// zero or only one previous point, backdate previous path points tangent to the car's angle:
							double prev_car_x = car_x - cos(car_yaw);
							double prev_car_y = car_y - sin(car_yaw);

							ptsx.push_back(prev_car_x);
							ptsx.push_back(car_x);
							ptsy.push_back(prev_car_y);
							ptsy.push_back(car_y);
						} else {
							// previous points available, take the second last and second last points and push to ptsx and ptsy and reset ref points
							ref_x = previous_path_x.back();
							ref_y = previous_path_y.back();
							double ref_x_second_last = previous_path_x[previous_path_size-2];
							double ref_y_second_last = previous_path_y[previous_path_size-2];
							// update ref_yaw using atan2
							ref_yaw = atan2(ref_y - ref_y_second_last, ref_x - ref_x_second_last);

							ptsx.push_back(ref_x_second_last);
							ptsx.push_back(ref_x);
							ptsy.push_back(ref_y_second_last);
							ptsy.push_back(ref_y);
						}

						// now create 3 new points based on frenet coordinates, 30m 60m and 90m apart
						vector<double> next_wp0 = getXY(car_s + 30, (double)2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);
						vector<double> next_wp1 = getXY(car_s + 60, (double)2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);
						vector<double> next_wp2 = getXY(car_s + 90, (double)2+4*lane, map_waypoints_s, map_waypoints_x, map_waypoints_y);

						ptsx.push_back(next_wp0[0]);
						ptsx.push_back(next_wp1[0]);
						ptsx.push_back(next_wp2[0]);

						ptsy.push_back(next_wp0[1]);
						ptsy.push_back(next_wp1[1]);
						ptsy.push_back(next_wp2[1]);

						// transform from map coordinates to vehicle coordinates, similar to the transform function in MPC project
						for(int i=0; i<ptsx.size(); i++) {
							// TODO: write transform() function
							double shift_x = ptsx[i] - ref_x;
							double shift_y = ptsy[i] - ref_y;

							ptsx[i] = shift_x * cos(-ref_yaw) - shift_y * sin(-ref_yaw);
							ptsy[i] = shift_x * sin(-ref_yaw) + shift_y * cos(-ref_yaw);
						}

						// spline for smoother path
						tk::spline s;

						s.set_points(ptsx, ptsy);

						// start with all the previous points
						for(int i=0; i<previous_path_size; i++) {
							next_x_vals.push_back(previous_path_x[i]);
							next_y_vals.push_back(previous_path_y[i]);
						}

						// calculate how many additional points based on the spline projection, and location of each point, to next_x_vals and next_y_vals
						double target_x = 30.0; // arbitrary
						double target_y = s(target_x);
						double target_dist = sqrt(target_x*target_x + target_y*target_y); // end point from car's current location until the spline point 30.0m along the x-axis (map coordinate)

						double x_addon = 0;

						for(int i=0; i<50-previous_path_size; i++) {
							if (too_close) {
								// first check if can change lanes
								if (can_turn_left && lane > 0) { //TODO: can probably put the lane > 0 logic above as a part of can_turn_left
									lane -= 1;
								} else if (can_turn_right && lane < 2) {
									lane += 1;
								} else {
									// otherwise, slow down
									ref_vel -= max_acc;
								}
							} else if (ref_vel < max_vel) {
								ref_vel += max_acc;
							}

							// N is how many evenly spaced projected waypoints between car's current position plus target_dist, so it's
							// target_dist divided by 0.02, which is time in seconds (20ms), multiplied by rel_vel in m/s
							double N = target_dist/(time_inc*ref_vel*dist_inc);
							double x_point = x_addon + target_x/N;
							double y_point = s(x_point);

							// reset x_addon
							x_addon = x_point;

							// local variables for transforming back to map coordinates from vehicle coordinates, not to be confused with ref_x and ref_y, // which are coordinates that help us get back to the map coordinates
							double x_ref = x_point;
							double y_ref = y_point;

							// back to map coordinates; TODO: write transform() function
							x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw)) + ref_x;
							y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw)) + ref_y;

							next_x_vals.push_back(x_point);
							next_y_vals.push_back(y_point);
						}

          	msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
