#include <cnoid/SimpleController>
#include <cnoid/Sensor>
#include <cnoid/BodyLoader>
#include <eigen3/Eigen/Eigen>
#include <ros/ros.h>
#include <ros/package.h>
#include "trajectory_planner/Trajectory.h"
#include "trajectory_planner/JntAngs.h"
#include "trajectory_planner/GeneralTraj.h"
#include "trajectory_planner/Robot.h"
#include "BumpSensor.h"
#include "std_srvs/Empty.h"
#include <std_msgs/Int32.h>
#include <tf/tf.h>
#include <nav_msgs/Path.h>

using namespace std;
using namespace cnoid;
using namespace Eigen;

// Surena IV

const double pgain[] = {
    8000.0, 8000.0, 8000.0, 8000.0, 8000.0, 8000.0,
    8000.0, 8000.0, 8000.0, 8000.0, 8000.0, 8000.0, 8000.0,
    8000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0,
    3000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0,
    3000.0, 3000.0, 3000.0};

const double dgain[] = {
    100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
    100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
    100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
    100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
    100.0, 100.0, 100.0};

// SurenaV

// const double pgain[] = {
//     10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0,
//     10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 10000.0, 8000.0,
//     8000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0,
//     3000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0, 3000.0,
//     3000.0, 3000.0, 3000.0 };

// const double dgain[] = {
//     20.0, 20.0, 20.0, 20.0, 20.0, 20.0,
//     20.0, 20.0, 20.0, 20.0, 20.0, 20.0, 100.0,
//     100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
//     100.0, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0,
//     100.0, 100.0, 100.0 };

class SurenaController : public SimpleController
{

    ros::NodeHandle nh;
    ros::Subscriber keyboardCommandSub_ = nh.subscribe("/keyboard_command", 1, &SurenaController::commandHandler, this);
    ros::Subscriber trajectoryCommandSub_ = nh.subscribe("/cmd_vel", 1, &SurenaController::localPlanner, this);
    ros::Publisher comDataPub = nh.advertise<geometry_msgs::Pose>("surena/robot_pose", 100);
  
    int idx = 0;
    double dt;
    double qref[29];
    double qold[29];
    BodyPtr ioBody;
    ForceSensor *leftForceSensor;
    ForceSensor *rightForceSensor;
    AccelerationSensor *accelSensor;
    RateGyroSensor *gyro;
    Robot *robot;
    int surenaIndex_[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    int sr1Index_[12] = {2, 0, 1, 3, 4, 5, 15, 13, 14, 16, 17, 18};

    double init_com_pos[3] = {0, 0, 0.71}, init_com_orient[3] = {0, 0, 0}, final_com_pos[3] = {0, 0, 0.71}, final_com_orient[3] = {0, 0, 0};
    double init_lankle_pos[3] = {0, 0.1, 0}, init_lankle_orient[3] = {0, 0, 0}, final_lankle_pos[3] = {0, 0.1, 0}, final_lankle_orient[3] = {0, 0, 0};
    double init_rankle_pos[3] = {0, -0.1, 0}, init_rankle_orient[3] = {0, 0, 0}, final_rankle_pos[3] = {0, -0.1, 0}, final_rankle_orient[3] = {0, 0, 0};
    double jnt_command[12];
    int status;

    double step_width = 0.0;
    double alpha = 0.44;
    bool use_file = false;
    double t_init_double_support = 1;
    double t_double_support = 0.1;
    double t_step = 0.9;
    double t_final_double_support = 1;
    double step_length = 0.2;
    double COM_height = 0.68;
    double step_count = 2;
    double ankle_height = 0.025;
    double step_height = 0;
    double theta = 0.0;
    double slope = 0.0;
    double offset = 0.0;
    bool is_config = false;
    bool isRunningTrajectory = false;

    int size_;

    double previous_pose[3] = {0, 0, 0}; 

public:
    void callGeneralTraj(double time)
    {
        robot->generalTrajGen(dt, time, init_com_pos, final_com_pos, init_com_orient, final_com_orient,
                              init_lankle_pos, final_lankle_pos, init_lankle_orient, final_lankle_orient,
                              init_rankle_pos, final_rankle_pos, init_rankle_orient, final_rankle_orient);
        idx = 0;
    }

    void callTraj()
    {
        robot->trajGen(step_count, t_step, alpha, t_double_support, COM_height, step_length, 
                       step_width, dt, theta, ankle_height, step_height, slope, offset, is_config);
        idx = 0;
    }

    void commandHandler(const std_msgs::Int32 &msg)
    {
        int command = msg.data;
        if (!this->isRunningTrajectory)
        {
            switch (command)
            {
            case 119: // w:move forward
                step_count = 2;
                step_length = 0.15;
                theta = 0.0;
                callTraj();
                break;

            case 115: // s:move backward
                step_count = 2;
                step_length = -0.15;
                theta = 0.0;
                callTraj();
                break;

            case 97: // a:turn left
                step_count = 2;
                step_length = -0.15;
                theta = 0.2;
                callTraj();
                break;

            case 100: // d:turn right
                step_count = 2;
                step_length = 0.15;
                theta = 0.2;
                callTraj();
                break;

            case 99: // c:config file trajectory
                is_config = true;
                callTraj();
                is_config = false;
                break;

            default:
                break;
            }
            isRunningTrajectory = true;
        }
    }

    void localPlanner(const geometry_msgs::Twist &msg)
    {
        if (!this->isRunningTrajectory)
        { 
            if (msg.linear.x > 0 && abs(msg.angular.z) < 0.05)
                {
                    step_length = msg.linear.x;
                    theta = 0.0;
                    step_count = 3;
                }
            else if (msg.linear.x > 0 && msg.angular.z > 0.05)
                {
                    step_length = -msg.linear.x;
                    theta = abs(msg.angular.z);
                    step_count = 3;
                }
            else if (msg.linear.x > 0 && msg.angular.z < -0.05)
                {
                    step_length = msg.linear.x;
                    theta = abs(msg.angular.z);
                    step_count = 3;
                }
            else if (msg.linear.x < 0)
                {
                    step_length = msg.linear.x;
                    theta = 0.0;
                    step_count = 2;
                }

            callTraj();
            isRunningTrajectory = true;  
        }
    }

    virtual bool initialize(SimpleControllerIO *io) override
    {
        dt = io->timeStep();
        final_com_pos[2] = 0.68;
        string config_path = ros::package::getPath("trajectory_planner") + "/config/surenav_config.json";
        robot = new Robot(&nh, config_path, true);
        callGeneralTraj(2);
        // callTraj();
        isRunningTrajectory = true;
        size_ = robot->getTrajSize();
        ioBody = io->body();
        leftForceSensor = ioBody->findDevice<ForceSensor>("LeftAnkleForceSensor");
        rightForceSensor = ioBody->findDevice<ForceSensor>("RightAnkleForceSensor");
        io->enableInput(leftForceSensor);
        io->enableInput(rightForceSensor);
        accelSensor = ioBody->findDevice<AccelerationSensor>("WaistAccelSensor");
        io->enableInput(accelSensor);
        gyro = ioBody->findDevice<RateGyroSensor>("WaistGyro");
        io->enableInput(gyro);
        io->enableInput(ioBody->link(0), LINK_POSITION);

        for (int i = 0; i < ioBody->numJoints(); ++i)
        {
            Link *joint = ioBody->joint(i);
            joint->setActuationMode(Link::JOINT_TORQUE);
            io->enableIO(joint);
            if (i == 5 || i == 11)
            {
                // Enabling Ankles Position IO (required by bump sensor)
                io->enableInput(joint, LINK_POSITION);
            }
            qref[i] = joint->q();
            qold[i] = qref[i];
        }
        return true;
    }
    virtual bool control() override
    {
        size_ = robot->getTrajSize();
        if (idx < size_ - 1)
        {
            double left_ft[] = {float(leftForceSensor->f().z()),
                                float(leftForceSensor->tau().x()),
                                float(leftForceSensor->tau().y())};
            double right_ft[] = {float(rightForceSensor->f().z()),
                                 float(rightForceSensor->tau().x()),
                                 float(rightForceSensor->tau().y())};

            int right_bump[] = {0, 0, 0};
            int left_bump[] = {0, 0, 0};

            double cur_q[ioBody->numJoints()];
            double jnt_vel[ioBody->numJoints()];
            for (int i = 0; i < ioBody->numJoints(); ++i)
            {
                Link *joint = ioBody->joint(i);
                cur_q[i] = joint->q();
                jnt_vel[i] = (cur_q[surenaIndex_[i]] - qold[surenaIndex_[i]]) / dt;
            }

            double accelerometer[] = {accelSensor->dv()(0), accelSensor->dv()(1), accelSensor->dv()(2)};
            double gyroscope[] = {gyro->w()(0), gyro->w()(1), gyro->w()(2)};

            Matrix4d l_ankle, r_ankle;
            l_ankle.block<3, 1>(0, 3) = ioBody->joint(11)->position().translation();
            r_ankle.block<3, 1>(0, 3) = ioBody->joint(5)->position().translation();
            l_ankle.block<3, 3>(0, 0) = ioBody->joint(11)->position().rotation();
            r_ankle.block<3, 3>(0, 0) = ioBody->joint(5)->position().rotation();

            Vector3d base_pos = ioBody->link(0)->p();
            Matrix3d base_rot = ioBody->link(0)->R();

            // rotation to quaternion
            Quaterniond base_quat(base_rot);

            // publish robot pose and orientation
            geometry_msgs::Pose com_pose;
            com_pose.position.x = base_pos(0);
            com_pose.position.y = base_pos(1);
            com_pose.position.z = base_pos(2);
            com_pose.orientation.x = base_quat.x();
            com_pose.orientation.y = base_quat.y();
            com_pose.orientation.z = base_quat.z();
            com_pose.orientation.w = base_quat.w();
            comDataPub.publish(com_pose);

            Vector3d left_ankle_pos = l_ankle.block<3, 1>(0, 3);
            Vector3d right_ankle_pos = r_ankle.block<3, 1>(0, 3);

            robot->getJointAngs(idx, cur_q, jnt_vel, right_ft, left_ft, right_bump,
                                left_bump, gyroscope, accelerometer, jnt_command, status);

            for (int j = 0; j < 12; j++)
                qref[surenaIndex_[j]] = jnt_command[j];

            for (int i = 0; i < ioBody->numJoints(); ++i)
            {
                Link *joint = ioBody->joint(i);
                double q = joint->q();
                double dq = (q - qold[i]) / dt;
                double u = (qref[i] - q) * pgain[i] + (0.0 - dq) * dgain[i];
                qold[i] = q;
                joint->u() = u;
            }
            idx++;
        }
        else
        {
            if (idx == size_ - 1)
            {
                isRunningTrajectory = false;
                robot->resetTraj();
            }
            for (int i = 0; i < ioBody->numJoints(); ++i)
            {
                Link *joint = ioBody->joint(i);
                double q = joint->q();
                double dq = (q - qold[i]) / dt;
                double u = (qref[i] - q) * pgain[i] + (0.0 - dq) * dgain[i];
                qold[i] = q;
                joint->u() = u;
            }
        }
        ros::spinOnce();
        return true;
    }
};
CNOID_IMPLEMENT_SIMPLE_CONTROLLER_FACTORY(SurenaController)