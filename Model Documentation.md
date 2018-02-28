##Reflection writeup

###Overview
This was a very difficult project for me as I struggled to come up with a reliable solution to efficiently go around the track without incidents, so I got plenty of help from the project walkthrough video, forums and other students' ideas and suggestions.

###The model
####Getting the car to run
I started by getting the car to run a straight line first, this required simply populating the the `next_x_vals` and `next_y_vals` vectors with points to form a projected path. I chose 50 points because the car moves 50 times a second, then set these points `0.447` meter apart from each other, so 50 points will form a projected path of 22.35 meters, which means the car will travel 22.35 meters per second, roughly equal to 50mph, the speed limit.

Once these are set, the car travels in a straight line and ignores all obstacles.

####Stay in lane
My next goal was to make the car stays in its lane by making smooth turns to follow the curvature of the lane. This requires setting a curved projected path using a polynomial. Since the waypoints are provided by the simulator in the forms of `map_waypoints_s`, `map_waypoints_x` and `map_waypoints_y`, I could use these data points, the spline library and the provided `getXY` function to form a curved path by using arbitrary points, in this case 30, 60 and 90 meters ahead of the vehicle. This generates a nice smooth curved path along the waypoints, which keep the car in lane.

Since the simulator provides points from the previously provided path, the projected path would be further smoothed by first populating the `next_x_vals` and `next_y_vals` vectors with previous path data, if available, and then filling the rest with new path data.

This will let the car stay in lane, but it still ignores obstacles, namely other cars, on the road and would collide into anything on its path.

####Avoid collision
The car should do two things to avoid collision with the car in front of it:

* 1) Slow down
* 2) Change lanes, when it's safe to do so to avoid colliding with vehicles in other lanes

Both behaviors should be modeled using one finite state machine (FSM), and this is done using 3 flags: `too_close`, `can_turn_left` and `can_turn_right`.

The first behavior, slowing down, is a lot easier to implement. Using sensor fusion data, we get the location (`s` and `d`) and velocity information of the cars near our vehicle. First we use `d` to determine the lane of the detected vehicle, then compare the lane of the detected vehicle with our car, if the detected vehicle is in our lane, we then use the `s` value to determine if the vehicle is in front of us, and it's projected position, based on its current velocity, is within 30 meters of our car, we mark `too_close` to true, which will then be used to gradually reduce the velocity as we generate the projected path by subtracting a `max_acc` value that does not exceed 10m/s^2 (in this case set to 5m/s^2) to ensure we do not exceed max acceleration and jerk.

We then do this for each detected vehicle to make sure that all potential vehicles ahead of us are accounted for.

The lane change logic works by assessing the following condition: the detected vehicle is to the lane left of us, then check that the vehicle's projected position is at least 30 meters away from our vehicle, and lastly, our car is not already at the leftmost lane. We then do this for all detected vehicles, and only if all vehicles satisfy the above condition, we mark `can_turn_left` to true.

The above process is repeated to construct the `can_turn_right` by replacing check left with check right.

The 3 lanes on the right hand side of the road are marked as 0 (left lane), 1 (middle lane) and 2 (right lane). If `can_turn_left` or `can_turn_right` if evaluated to true, we subtract/add 1 from/to our car's `lane` variable, which is then used to construct the vehicle's `d`. `can_turn_left` is evaluated first so a preference is given to change to the lane on the left to pass a vehicle.

####Drive close to speed limit
Since the car will begin to slow down when `too_close` evaluates to true, the reverse logic should be implemented to ensure the car picks up speed when it's safe to do so. Therefore, if `too_close` evaluates to false and the car's current velocity is below the speed limit, we add `max_acc` value to the car's velocity.

###Reflection
The car is able to complete the track without breaking the constraints, i.e. drive 4.32 miles without incidents, does not exceed the speed limit, max acceleration or jerk, does not collide with other vehicles, and able to change lanes but stays in lane otherwise. However, several improvements can be made to the model:

* 1) Instead of blindly slowing down the car when it's too close to the vehicle in front, we can smooth it out further by matching the speed of the vehicle in front of us
* 2) Use a cost optimizer to model the trade-off between speed, acceleration, and lane change (similar to the MPC project in term 2)
* 3) The vehicle should try to stay in the rightmost lane unless it's passing other vehicles