# Raspberry Pi
Assignment 1:
	i. Neck control to pan head in a sinusoidal manner (side to side) where the
		head moves fastest in the centre of the range.

	ii. Eye control from host computer (key press selection of each)

	1. Show stereo control by adjusting PWM to each eye and scan
		horizontal axis at a plausible rate, mimicking human eye motion
		following a target.

	2. Show chameleon like eye motion for 10 seconds.

	3. Show two other behaviours that mimic human or animal emotive
		eye motion.

Assignment 2:
	– Stereo vision software & eye control

	• Using simple Cross-correlation & Servo control
	– Verge onto a slow-moving target with both eyes,
	– Give estimate of distance to user, report distance against
	measured
	– Track target for 10 seconds over whole field of view of robot

	• Using Homography in OpenCV & servo control
	– Calibrate cameras in orthographic mode
	– Show live disparity images,
	– Calculate disparity of a target, produce depth map,
		demonstrate calibration against ruler, assess errors

	• (40%) Using Itti & Koch’s bottom-up Saccadic model of stereo eye
	control, develop a processing model and apply it to process a scene.
	The model should Saccade to salient targets, acquire images, map
	distances to targets, on a live stereo video stream.

	• (20%) Report on all above, with references and comparisons between
	your results above and with animal stereo systems
	• (5% ) Review availab