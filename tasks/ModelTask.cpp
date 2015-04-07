/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */
//======================================================================================
// Brazilian Institute of Robotics 
// Authors: Thomio Watanabe
// Date: December 2014
//====================================================================================== 

#include "ModelTask.hpp"

using namespace gazebo;
using namespace rock_gazebo;
using namespace std;

ModelTask::ModelTask(string const& name)
	: ModelTaskBase(name)
{
}

ModelTask::ModelTask(string const& name, RTT::ExecutionEngine* engine)
	: ModelTaskBase(name, engine)
{
}

ModelTask::~ModelTask()
{
    for(ExportedLinks::iterator it = exported_links.begin(); it != exported_links.end(); ++it)
        delete it->port;
}

void ModelTask::setGazeboModel(WorldPtr _world,  ModelPtr _model)
{
    string name = "gazebo:" + _world->GetName() + ":" + _model->GetName();
    provides()->setName(name);
    _name.set(name);

    world = _world;
    model = _model;
} 

void ModelTask::setupJoints()
{
    // Get all joints from a model and set Rock Input/Output Ports
    gazebo_joints = model->GetJoints();
    for(Joint_V::iterator joint = gazebo_joints.begin(); joint != gazebo_joints.end(); ++joint)
    {
        gzmsg << "ModelTask: found joint: " << world->GetName() + "/" + model->GetName() +
                "/" + (*joint)->GetName() << endl;

        joints_in.names.push_back( (*joint)->GetName() );
        joints_in.elements.push_back( base::JointState::Effort(0.0) );
    }
}

void ModelTask::setupLinks()
{
    // The robot configuration YAML file must define the exported links.
    vector<LinkExport> export_conf = _exported_links.get();
    for(vector<LinkExport>::iterator it = export_conf.begin();
            it != export_conf.end(); ++it)
    {
        ExportedLink exported_link;

        exported_link.source_link =
            checkExportedLinkElements("source_link", it->source_link, "world");
        exported_link.target_link =
            checkExportedLinkElements("target_link", it->target_link, "world");
        exported_link.source_frame =
            checkExportedLinkElements("source_frame", it->source_frame, it->source_link);
        exported_link.target_frame =
            checkExportedLinkElements("target_frame", it->target_frame, it->target_link);
        exported_link.source_link_ptr = model->GetLink( it->source_link );
        exported_link.target_link_ptr = model->GetLink( it->target_link );

        if (it->source_link != "world" && !exported_link.source_link_ptr)
        {
            gzmsg << "ModelTask: cannot find link " << it->source_link << " in model" << endl;
            gzmsg << "Exiting simulation." << endl;
        }
        else if (it->target_link != "world" && !exported_link.target_link_ptr)
        {
            gzmsg << "ModelTask: cannot find link " << it->target_link << " in model" << endl;
            gzmsg << "Exiting simulation." << endl;
        }
        else
        {
            // Create the ports dynamicaly
            gzmsg << "ModelTask: exporting link to rock: " << world->GetName() + "/" + model->GetName() +
                "/" + exported_link.source_link_ptr->GetName() << "2" << exported_link.target_link_ptr->GetName() << endl;

            exported_link.port = new RBSOutPort( it->source_frame + "2" + it->target_frame );
            ports()->addPort(*exported_link.port);
            exported_links.push_back(exported_link);
        }
    }
}

void ModelTask::updateHook()
{
    updateJoints();
    updateLinks();
}

void ModelTask::updateJoints()
{
    _joints_cmd.readNewest( joints_in );

    vector<string> names;
    vector<double> positions;

    for(Joint_V::iterator it = gazebo_joints.begin(); it != gazebo_joints.end(); ++it )
    {
        // Apply effort to joint
        if( joints_in.getElementByName( (*it)->GetName() ).hasEffort() )
            (*it)->SetForce(0, joints_in.getElementByName( (*it)->GetName() ).effort );

        // Read joint angle from gazebo link
        names.push_back( (*it)->GetName() );
        positions.push_back( (*it)->GetAngle(0).Radian() );
    }
    _joints_samples.write( base::samples::Joints::Positions(positions,names) );
}

void ModelTask::updateLinks()
{
    for(ExportedLinks::const_iterator it = exported_links.begin(); it != exported_links.end(); ++it)
    {
        math::Pose source_pose = math::Pose::Zero;
        if (it->source_link_ptr)
            source_pose = it->source_link_ptr->GetWorldPose();
        math::Pose target_pose = math::Pose::Zero;
        if (it->target_link_ptr)
            target_pose = it->target_link_ptr->GetWorldPose();
        math::Pose relative_pose( math::Pose(source_pose - target_pose) );

        RigidBodyState rbs;
        rbs.sourceFrame = it->source_frame;
        rbs.targetFrame = it->target_frame;
        rbs.position = base::Vector3d(
            relative_pose.pos.x,relative_pose.pos.y,relative_pose.pos.z);
        rbs.orientation = base::Quaterniond(
            relative_pose.rot.w,relative_pose.rot.x,relative_pose.rot.y,relative_pose.rot.z );

        it->port->write(rbs);
    }
}

bool ModelTask::configureHook()
{
    if( ! ModelTaskBase::configureHook() )
        return false;

    // Test if setGazeboModel() has been called -> if world/model are NULL
    if( (!world) && (!model) )
        return false;

    setupLinks();
    setupJoints();

    return true;
}

string ModelTask::checkExportedLinkElements(string element_name, string test, string option)
{
    // when not defined, source_link and target_link will recieve "world".
    // when not defined, source_frame and target_frame will receive source_link and target_link content
    if( test.empty() )
    {
        gzmsg << "ModelTask: " << model->GetName() << " " << element_name << " not defined, using "<< option << endl;
        return option;
    }else {
        gzmsg << "ModelTask: " << model->GetName() << " " << element_name << ": " << test << endl;
        return test;
    }
}

