/*
 * FAR Planner
 * Copyright (C) 2021 Fan Yang - All rights reserved
 * fanyang2@andrew.cmu.edu,   
 */



#include "far_planner/planner_visualizer.h"
#include "far_planner/polygon.h"
#include <nav_msgs/OccupancyGrid.h>
/***************************************************************************************/


void DPVisualizer::Init(const ros::NodeHandle& nh) {
    nh_ = nh;
    point_cloud_ptr_ = PointCloudPtr(new pcl::PointCloud<PCLPoint>());
    // Rviz Publisher
    viz_node_pub_    = nh_.advertise<Marker>("/viz_node_topic", 5);
    viz_path_pub_    = nh_.advertise<Marker>("/viz_path_topic", 5);
    viz_poly_pub_    = nh_.advertise<MarkerArray>("/viz_poly_topic", 5);
    viz_graph_pub_   = nh_.advertise<MarkerArray>("/viz_graph_topic", 5);
    viz_contour_pub_ = nh_.advertise<MarkerArray>("/viz_contour_topic", 5);
    viz_map_pub_     = nh_.advertise<MarkerArray>("/viz_grid_map_topic", 5);
    viz_view_extend  = nh_.advertise<MarkerArray>("/viz_viewpoint_extend_topic", 5);
    map_pub = nh_.advertise<nav_msgs::OccupancyGrid>("grid_map", 1);
}

void DPVisualizer::VizNodes(const NodePtrStack& node_stack, 
                            const std::string& ns,
                            const VizColor& color,
                            const float scale,
                            const float alpha)
{
    Marker node_marker;
    node_marker.type = Marker::SPHERE_LIST;
    this->SetMarker(color, ns, scale, alpha, node_marker);
    node_marker.points.resize(node_stack.size());
    std::size_t idx = 0;
    for (const auto& node_ptr : node_stack) {
        if (node_ptr == NULL) continue;
        node_marker.points[idx] = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        idx ++;
    }
    node_marker.points.resize(idx);
    viz_node_pub_.publish(node_marker);
}

void DPVisualizer::VizPoint3D(const Point3D& point, 
                             const std::string& ns,
                             const VizColor& color,
                             const float scale,
                             const float alpha)
{
    Marker node_marker;
    node_marker.type = Marker::SPHERE;
    this->SetMarker(color, ns, scale, alpha, node_marker);
    std::size_t idx = 0;
    node_marker.pose.position.x = point.x;
    node_marker.pose.position.y = point.y;
    node_marker.pose.position.z = point.z;
    viz_node_pub_.publish(node_marker);
}

void DPVisualizer::VizPath(const NodePtrStack& global_path, const bool& is_free_nav) {
    Marker path_marker;
    path_marker.type = Marker::LINE_STRIP;
    const VizColor color = is_free_nav ? VizColor::GREEN : VizColor::BLUE;
    this->SetMarker(color, "global_path", 0.75f, 0.9f, path_marker);
    geometry_msgs::Point geo_p;
    for (const auto& node_ptr : global_path) {
        geo_p = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        path_marker.points.push_back(geo_p);
    }
    viz_path_pub_.publish(path_marker);
}

void DPVisualizer::VizViewpointExtend(const NavNodePtr& ori_nav_ptr, const Point3D& extend_point) {
    MarkerArray view_extend_marker_array;
    Marker corner_direct_marker, ray_tracing_marker, origin_p_marker, extend_p_marker;
    corner_direct_marker.type = Marker::LINE_LIST;
    ray_tracing_marker.type   = Marker::LINE_LIST;
    origin_p_marker.type      = Marker::SPHERE_LIST;
    extend_p_marker.type      = Marker::SPHERE_LIST;
    this->SetMarker(VizColor::EMERALD, "origin_viewpoint", 0.7f,  0.5f,   origin_p_marker);
    this->SetMarker(VizColor::RED,     "extend_viewpoint", 0.7f,  0.5f,   extend_p_marker);
    this->SetMarker(VizColor::YELLOW,  "raytracing_line",  0.3f,  0.5f,   ray_tracing_marker);
    this->SetMarker(VizColor::MAGNA,   "corner_direct",    0.15f, 0.75f,  corner_direct_marker);
    geometry_msgs::Point p_start, p_end;
    p_start = FARUtil::Point3DToGeoMsgPoint(ori_nav_ptr->position);
    p_end   = FARUtil::Point3DToGeoMsgPoint(extend_point);
    origin_p_marker.points.push_back(p_start);
    extend_p_marker.points.push_back(p_end);
    // ray tracing marker
    ray_tracing_marker.points.push_back(p_start), ray_tracing_marker.points.push_back(p_end);
    auto Draw_Surf_Dir = [&](const NavNodePtr& node_ptr) {
        geometry_msgs::Point p1, p2, p3;
        p1 = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        Point3D end_p;
        if (node_ptr->free_direct != NodeFreeDirect::PILLAR) {
            end_p = node_ptr->position + node_ptr->surf_dirs.first * FARUtil::kVizRatio;
            p2 = FARUtil::Point3DToGeoMsgPoint(end_p);
            corner_direct_marker.points.push_back(p1);
            corner_direct_marker.points.push_back(p2);
            end_p = node_ptr->position + node_ptr->surf_dirs.second * FARUtil::kVizRatio;
            p3 = FARUtil::Point3DToGeoMsgPoint(end_p);
            corner_direct_marker.points.push_back(p1);
            corner_direct_marker.points.push_back(p3);
        }
    };
    Draw_Surf_Dir(ori_nav_ptr);
    view_extend_marker_array.markers.push_back(corner_direct_marker);
    view_extend_marker_array.markers.push_back(ray_tracing_marker);
    view_extend_marker_array.markers.push_back(origin_p_marker);
    view_extend_marker_array.markers.push_back(extend_p_marker);
    viz_view_extend.publish(view_extend_marker_array);
}

void DPVisualizer::VizGlobalPolygons(const std::vector<PointPair>& contour_pairs, const std::vector<PointPair>& unmatched_pairs) {
    MarkerArray poly_marker_array;
    Marker global_contour_marker, unmatched_contour_marker;
    global_contour_marker.type    = Marker::LINE_LIST;
    unmatched_contour_marker.type = Marker::LINE_LIST;
    this->SetMarker(VizColor::ORANGE, "global_contour",    0.2f,  0.5f, global_contour_marker);
    this->SetMarker(VizColor::YELLOW, "unmatched_contour", 0.15f, 0.5f, unmatched_contour_marker);
    for (const auto& p_pair : contour_pairs) {
        geometry_msgs::Point p_start = FARUtil::FARUtil::Point3DToGeoMsgPoint(p_pair.first);
        geometry_msgs::Point p_end   = FARUtil::FARUtil::Point3DToGeoMsgPoint(p_pair.second);
        global_contour_marker.points.push_back(p_start);
        global_contour_marker.points.push_back(p_end);
    }
    for (const auto& p_pair : unmatched_pairs) {
        geometry_msgs::Point p_start = FARUtil::FARUtil::Point3DToGeoMsgPoint(p_pair.first);
        geometry_msgs::Point p_end   = FARUtil::FARUtil::Point3DToGeoMsgPoint(p_pair.second);
        unmatched_contour_marker.points.push_back(p_start);
        unmatched_contour_marker.points.push_back(p_end);
    }
    poly_marker_array.markers.push_back(global_contour_marker);
    poly_marker_array.markers.push_back(unmatched_contour_marker);
    viz_poly_pub_.publish(poly_marker_array);
}

void DPVisualizer::VizContourGraph(const CTNodeStack& contour_graph) 
{
    MarkerArray contour_marker_array;
    Marker contour_vertex_marker, vertex_matched_marker, necessary_vertex_marker;
    Marker contour_marker, contour_surf_marker, contour_helper_marker;
    contour_vertex_marker.type    = Marker::SPHERE_LIST;
    vertex_matched_marker.type    = Marker::SPHERE_LIST;
    necessary_vertex_marker.type  = Marker::SPHERE_LIST;
    contour_marker.type         = Marker::LINE_LIST;
    contour_surf_marker.type    = Marker::LINE_LIST;
    contour_helper_marker.type  = Marker::CUBE_LIST;
    this->SetMarker(VizColor::EMERALD, "polygon_vertex",   0.5f, 0.5f,   contour_vertex_marker);
    this->SetMarker(VizColor::RED,     "matched_vertex",   0.5f, 0.5f,   vertex_matched_marker);
    this->SetMarker(VizColor::GREEN,   "necessary_vertex", 0.5f, 0.5f,   necessary_vertex_marker);
    this->SetMarker(VizColor::MAGNA,   "contour",          0.1f, 0.25f,  contour_marker);
    this->SetMarker(VizColor::BLUE,    "vertex_angle",     0.15f, 0.75f, contour_surf_marker);
    this->SetMarker(VizColor::BLUE,    "angle_direct",     0.25f, 0.75f, contour_helper_marker);

    auto Draw_Contour = [&](const CTNodePtr& ctnode_ptr) {
        geometry_msgs::Point geo_vertex, geo_connect;
        geo_vertex = FARUtil::Point3DToGeoMsgPoint(ctnode_ptr->position);
        contour_vertex_marker.points.push_back(geo_vertex);
        if (ctnode_ptr->is_global_match) {
            vertex_matched_marker.points.push_back(geo_vertex);
        }
        if (ctnode_ptr->is_contour_necessary) {
            necessary_vertex_marker.points.push_back(geo_vertex);
        }
        if (ctnode_ptr->front == NULL || ctnode_ptr->back == NULL) return;
        contour_marker.points.push_back(geo_vertex);
        geo_connect = FARUtil::Point3DToGeoMsgPoint(ctnode_ptr->front->position);
        contour_marker.points.push_back(geo_connect);
        contour_marker.points.push_back(geo_vertex);
        geo_connect = FARUtil::Point3DToGeoMsgPoint(ctnode_ptr->back->position);
        contour_marker.points.push_back(geo_connect);
    };
    auto Draw_Surf_Dir = [&](const CTNodePtr& ctnode) {
        geometry_msgs::Point p1, p2, p3;
        p1 = FARUtil::Point3DToGeoMsgPoint(ctnode->position);
        Point3D end_p;
        if (ctnode->free_direct != NodeFreeDirect::PILLAR) {
            end_p = ctnode->position + ctnode->surf_dirs.first * FARUtil::kVizRatio;
            p2 = FARUtil::Point3DToGeoMsgPoint(end_p);
            contour_surf_marker.points.push_back(p1);
            contour_surf_marker.points.push_back(p2);
            contour_helper_marker.points.push_back(p2);
            end_p = ctnode->position + ctnode->surf_dirs.second * FARUtil::kVizRatio;
            p3 = FARUtil::Point3DToGeoMsgPoint(end_p);
            contour_surf_marker.points.push_back(p1);
            contour_surf_marker.points.push_back(p3);
            contour_helper_marker.points.push_back(p3);
        }
    };
    //这是两个临时函数，并且这里用了 “[&]” 表示函数内的变量和函数外的一样，这两个函数是只读外部的变量（可以加关键字来修改），
    for (const auto& ctnode : contour_graph) {
        if (ctnode == NULL) {
            // DEBUG
            // ROS_ERROR("Viz: contour node is NULL.");
            continue;
        }
        Draw_Contour(ctnode);
        Draw_Surf_Dir(ctnode);
    }
    contour_marker_array.markers.push_back(contour_vertex_marker);
    contour_marker_array.markers.push_back(vertex_matched_marker);
    contour_marker_array.markers.push_back(necessary_vertex_marker);
    contour_marker_array.markers.push_back(contour_marker);
    contour_marker_array.markers.push_back(contour_surf_marker);
    contour_marker_array.markers.push_back(contour_helper_marker);
    //一组是        头（x，y） 到 尾（x，y）
    std::vector<std::vector<double>>  All_data;
    std::vector<std::vector<double>>  Simplified_data;
    std::vector<std::vector<std::vector<double>>>  Circle_data;
    int tem_h=0;
    std::vector<double>  tem_vec(4);
    for(int i=0;i<contour_marker.points.size();)//这个for循环里面完成了接受所有点，并且如果有相同的方向会一样
    {
        tem_vec[tem_h] = contour_marker.points[i].x;
        tem_vec[tem_h+1] = contour_marker.points[i].y;
        tem_h = tem_h+2;
        i++;
        if(i%2==0)
        {
            if(tem_vec[0]>tem_vec[2] || (tem_vec[0]==tem_vec[2]&&tem_vec[1]>tem_vec[3]))
            {
                double tem_var;
                tem_var=tem_vec[0];
                tem_vec[0]=tem_vec[2];
                tem_vec[2]=tem_var;
                tem_var=tem_vec[1];
                tem_vec[1]=tem_vec[3];
                tem_vec[3]=tem_var;
            }
            All_data.push_back(tem_vec);
            tem_h=0;
        }
    }
    Simplified_data=All_data;
    bool tem_flag=true;
    for(int i=0;i<Simplified_data.size();i++)
    {
        for(int h=i+1;h<Simplified_data.size();h++)
        {
            tem_flag=true;
            for(int k=0;k<4;k++)
            {
                if(Simplified_data[i][k]!=Simplified_data[h][k])
                {
                    tem_flag=false;
                    break;
                }
            }
            if(tem_flag)
            {
                Simplified_data.erase(Simplified_data.begin() + h); // 移除第h个元素，即索引为h的元素
                h--;
            }
        }
    }
    int tem_current_count = 0;
    int tem_current_count_inner = 0;
    bool tem_find_flag = false;
    tem_vec = Simplified_data[0];
    Simplified_data.erase(Simplified_data.begin());
    Circle_data.push_back({tem_vec});
    while(Simplified_data.size()!=0)
    {
        for(int i=0;i<Simplified_data.size();i++)//去寻找那个点，如果找到了就安装到上面去，并且当找到最后一个能首尾相接后break，
        {
            tem_find_flag = false;
            if(Circle_data[tem_current_count][tem_current_count_inner][2]==Simplified_data[i][0])
            {
                if(Circle_data[tem_current_count][tem_current_count_inner][3]==Simplified_data[i][1])
                {
                    tem_find_flag=true;
                }
            }
            if(Circle_data[tem_current_count][tem_current_count_inner][2]==Simplified_data[i][2])
            {
                if(Circle_data[tem_current_count][tem_current_count_inner][3]==Simplified_data[i][3])
                {
                    tem_find_flag=true;
                    double tem_var;
                    tem_var=Simplified_data[i][0];
                    Simplified_data[i][0]=Simplified_data[i][2];
                    Simplified_data[i][2]=tem_var;
                    tem_var=Simplified_data[i][1];
                    Simplified_data[i][1]=Simplified_data[i][3];
                    Simplified_data[i][3]=tem_var;
                }
            }//这里如果是后两位在
            
            if(tem_find_flag)
            {
                tem_vec = Simplified_data[i];
                Simplified_data.erase(Simplified_data.begin()+i);
                Circle_data[tem_current_count].push_back(tem_vec);
                tem_current_count_inner++;
                if(Circle_data[tem_current_count][tem_current_count_inner][2]==Circle_data[tem_current_count][0][0]
                && Circle_data[tem_current_count][tem_current_count_inner][3]==Circle_data[tem_current_count][0][1]
                && !Simplified_data.empty())
                {
                    std::vector<std::vector<double>>  tem_vec1;
                    tem_vec = Simplified_data[0];
                    Simplified_data.erase(Simplified_data.begin());
                    tem_vec1.push_back(tem_vec);
                    Circle_data.push_back(tem_vec1);
                    tem_current_count++;
                    tem_current_count_inner=0;
                    // std::cout<<"44444444444444444444444444444444||||||||||    "<<Circle_data.size()<<"  "<<std::endl;
                    break;
                }
            }
        }
    }
    std::vector<std::vector<Point2d>>  Points(Circle_data.size());
    // Points=Circle_data;
    Point2d tem_point2d(0,0);
    for(int i=0;i<Circle_data.size();i++)
    {
        for(int h=0;h<Circle_data[i].size();h++)
        {
            tem_point2d.x=Circle_data[i][h][0];
            tem_point2d.y=Circle_data[i][h][1];
            Points[i].push_back(tem_point2d);
        }
        tem_point2d.x=Circle_data[i][0][0];
        tem_point2d.y=Circle_data[i][0][1];
        Points[i].push_back(tem_point2d);
    }
    std::vector<MY_Polygon> Polygon_;
    double xmin,ymin,xmax,ymax;
    xmin = std::numeric_limits<double>::max();
    xmax = std::numeric_limits<double>::min();
    ymin = std::numeric_limits<double>::max();
    ymax = std::numeric_limits<double>::min();
    for(int i=0;i<Points.size();i++)//探取地图边界
    {
        MY_Polygon polygon_(Points[i]);
        Polygon_.push_back(polygon_);
        if(polygon_.bbox.xmin<xmin)
        {
            xmin = polygon_.bbox.xmin;
        }
        if(polygon_.bbox.xmax>xmax)
        {
            xmax = polygon_.bbox.xmax;
        }
        if(polygon_.bbox.ymin<ymin)
        {
            ymin = polygon_.bbox.ymin;
        }
        if(polygon_.bbox.ymax>ymax)
        {
            ymax = polygon_.bbox.ymax;
        }
    }


    nav_msgs::OccupancyGrid grid_map;
    grid_map.header.frame_id = "map"; // 设置地图的坐标系
    grid_map.info.resolution = 0.05;  // 设置栅格分辨率                   这里表示每个栅格长宽是0.01m
    grid_map.info.width = int((xmax-xmin)*1.2/grid_map.info.resolution);       // 设置地图宽度（以栅格为单位）
    grid_map.info.height = int((ymax-ymin)*1.2/grid_map.info.resolution);      // 设置地图高度（以栅格为单位）
    grid_map.data.resize(grid_map.info.width * grid_map.info.height, 0); // 初始化地图数据
    grid_map.info.origin.position.x = xmin;  // 设置地图原点坐标为正中央
    grid_map.info.origin.position.y = ymin;

    Point2d test_point(0,0);
    int ___coutn=0;
    int _coutn=0;
    for(int i=0;i<Polygon_.size();i++)//遍历每个多边形，遍历多边形bbox框住的像素点  判断是否在多边形内， 然后涂黑
    {
        for(double x_lo=Polygon_[i].bbox.xmin;x_lo<Polygon_[i].bbox.xmax;x_lo=x_lo+grid_map.info.resolution)//开始遍历bbox点
        {
            for(double y_lo=Polygon_[i].bbox.ymin+grid_map.info.resolution;y_lo<Polygon_[i].bbox.ymax;y_lo=y_lo+grid_map.info.resolution)
            {
                test_point.x=x_lo;
                test_point.y=y_lo;
                if(Polygon_[i].inPolygon(test_point))
                {
                    int index=(x_lo-grid_map.info.origin.position.x)/grid_map.info.resolution+grid_map.info.width*(y_lo-grid_map.info.origin.position.y)/grid_map.info.resolution;
                    grid_map.data[index]=100;
                    ___coutn++;
                }
                _coutn++;
            }
        }
    }
    std::cout<<___coutn<<"  "<<_coutn<<std::endl;
    map_pub.publish(grid_map);
    // std::cout<<"zzzzzzzzzzzzzzzzzzzz"<<std::endl;
    // for(int i=0;i<Circle_data.size();i++)
    // {
    //     std::cout<<std::endl;
    //     for(int h=0;h<Circle_data[i].size();h++)
    //     {
    //         std::cout<<"("<<Circle_data[i][h][0]<<","<<Circle_data[i][h][1]<<")  ("<<Circle_data[i][h][2]<<","<<Circle_data[i][h][3]<<")__";
    //     }
    // }
    // for(int i=0;i<Points.size();i++)
    // {
    //     std::cout<<std::endl;
    //     for(int h=0;h<Points[i].size();h++)
    //     {
    //         std::cout<<"("<<Points[i][h].x<<","<<Points[i][h].y<<")__";
    //     }
    // }
    // for(int i=0;i<10;i++)
    // {
    //     std::cout<<"----------------------"<<std::endl;
    // }
    // std::vector<double,double>
    //思路      1.先去除一样的线段
    //思路      2.再找到线连起来，闭合回路
    viz_contour_pub_.publish(contour_marker_array);
}

void DPVisualizer::VizGraph(const NodePtrStack& graph) {
    MarkerArray graph_marker_array;
    Marker nav_node_marker, unfinal_node_marker, near_node_marker, covered_node_marker, internav_node_marker, frontier_node_marker,
           edge_marker, visual_edge_marker, contour_edge_marker, free_edge_marker, odom_edge_marker, goal_edge_marker, traj_edge_marker,
           corner_surf_marker, contour_align_marker, corner_helper_marker, boundary_node_marker, boundary_edge_marker;
    nav_node_marker.type       = Marker::SPHERE_LIST;
    unfinal_node_marker.type   = Marker::SPHERE_LIST;
    near_node_marker.type      = Marker::SPHERE_LIST;
    covered_node_marker.type   = Marker::SPHERE_LIST;
    internav_node_marker.type  = Marker::SPHERE_LIST;
    boundary_node_marker.type  = Marker::SPHERE_LIST;
    frontier_node_marker.type  = Marker::SPHERE_LIST;
    contour_align_marker.type  = Marker::LINE_LIST;
    edge_marker.type           = Marker::LINE_LIST;
    visual_edge_marker.type    = Marker::LINE_LIST;
    free_edge_marker.type      = Marker::LINE_LIST;
    contour_edge_marker.type   = Marker::LINE_LIST;
    odom_edge_marker.type      = Marker::LINE_LIST;
    goal_edge_marker.type      = Marker::LINE_LIST;
    traj_edge_marker.type      = Marker::LINE_LIST;
    boundary_edge_marker.type  = Marker::LINE_LIST;
    corner_surf_marker.type    = Marker::LINE_LIST;
    corner_helper_marker.type  = Marker::CUBE_LIST;
    this->SetMarker(VizColor::WHITE,   "global_vertex",     0.5f,  0.5f,  nav_node_marker);
    this->SetMarker(VizColor::RED,     "updating_vertex",   0.5f,  0.8f,  unfinal_node_marker);
    this->SetMarker(VizColor::MAGNA,   "localrange_vertex", 0.5f,  0.8f,  near_node_marker);
    this->SetMarker(VizColor::BLUE,    "freespace_vertex",  0.5f,  0.8f,  covered_node_marker);
    this->SetMarker(VizColor::YELLOW,  "trajectory_vertex", 0.5f,  0.8f,  internav_node_marker);
    this->SetMarker(VizColor::GREEN,   "boundary_vertex",   0.5f,  0.8f,  boundary_node_marker);
    this->SetMarker(VizColor::ORANGE,  "frontier_vertex",   0.5f,  0.8f,  frontier_node_marker);
    this->SetMarker(VizColor::WHITE,   "global_vgraph",     0.1f,  0.2f,  edge_marker);
    this->SetMarker(VizColor::EMERALD, "freespace_vgraph",  0.1f,  0.25f, free_edge_marker);
    this->SetMarker(VizColor::EMERALD, "visibility_edge",   0.1f,  0.25f, visual_edge_marker);
    this->SetMarker(VizColor::RED,     "polygon_edge",      0.15f, 0.25f, contour_edge_marker);
    this->SetMarker(VizColor::ORANGE,  "boundary_edge",     0.2f,  0.25f, boundary_edge_marker);
    this->SetMarker(VizColor::ORANGE,  "odom_edge",         0.1f,  0.15f, odom_edge_marker);
    this->SetMarker(VizColor::YELLOW,  "to_goal_edge",      0.1f,  0.15f, goal_edge_marker);
    this->SetMarker(VizColor::GREEN,   "trajectory_edge",   0.1f,  0.5f,  traj_edge_marker);
    this->SetMarker(VizColor::YELLOW,  "vertex_angle",      0.15f, 0.75f, corner_surf_marker);
    this->SetMarker(VizColor::YELLOW,  "angle_direct",      0.25f, 0.75f, corner_helper_marker);
    this->SetMarker(VizColor::YELLOW,  "vertices_matches",  0.1f,  0.75f, contour_align_marker);
    /* Lambda Function */
    auto Draw_Contour_Align = [&](const NavNodePtr& node_ptr) {
        if (node_ptr->is_odom || !node_ptr->is_contour_match) return;
        geometry_msgs::Point nav_pos, vertex_pos;
        nav_pos = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        vertex_pos = FARUtil::Point3DToGeoMsgPoint(node_ptr->ctnode->position);
        contour_align_marker.points.push_back(vertex_pos);
        contour_align_marker.points.push_back(nav_pos);
    };
    auto Draw_Edge = [&](const NavNodePtr& node_ptr) {
        geometry_msgs::Point p1, p2;
        p1 = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        // navigable vgraph
        for (const auto& cnode : node_ptr->connect_nodes) {
            if (node_ptr->is_boundary && cnode->is_boundary &&  
                node_ptr->invalid_boundary.find(cnode->id) != node_ptr->invalid_boundary.end()) 
            {
                continue;
            }
            p2 = FARUtil::Point3DToGeoMsgPoint(cnode->position);
            edge_marker.points.push_back(p1);
            edge_marker.points.push_back(p2);
        }
        // poly edges
        for (const auto& cnode : node_ptr->poly_connects) {
            p2 = FARUtil::Point3DToGeoMsgPoint(cnode->position);
            if (FARUtil::IsOutsideGoal(node_ptr) || FARUtil::IsOutsideGoal(cnode)) {
                goal_edge_marker.points.push_back(p1);
                goal_edge_marker.points.push_back(p2);
            } else if (node_ptr->is_odom || cnode->is_odom) {
                odom_edge_marker.points.push_back(p1);
                odom_edge_marker.points.push_back(p2);
            } else {
                visual_edge_marker.points.push_back(p1);
                visual_edge_marker.points.push_back(p2);
                if (node_ptr->is_covered && cnode->is_covered) {
                    free_edge_marker.points.push_back(p1);
                    free_edge_marker.points.push_back(p2);
                }
            }
        }
        // contour edges
        for (const auto& ct_cnode : node_ptr->contour_connects) {
            p2 = FARUtil::Point3DToGeoMsgPoint(ct_cnode->position);
            contour_edge_marker.points.push_back(p1);
            contour_edge_marker.points.push_back(p2);
            if (node_ptr->is_boundary && ct_cnode->is_boundary) {
                boundary_edge_marker.points.push_back(p1);
                boundary_edge_marker.points.push_back(p2);
            }
        }
        // inter navigation trajectory connections
        if (node_ptr->is_navpoint) {
            for (const auto& tj_cnode : node_ptr->trajectory_connects) {
                p2 = FARUtil::Point3DToGeoMsgPoint(tj_cnode->position);
                traj_edge_marker.points.push_back(p1);
                traj_edge_marker.points.push_back(p2);
            }
        }
    };
    auto Draw_Surf_Dir = [&](const NavNodePtr& node_ptr) {
        geometry_msgs::Point p1, p2, p3;
        p1 = FARUtil::Point3DToGeoMsgPoint(node_ptr->position);
        Point3D end_p;
        if (node_ptr->free_direct != NodeFreeDirect::PILLAR) {
            end_p = node_ptr->position + node_ptr->surf_dirs.first * FARUtil::kVizRatio;
            p2 = FARUtil::Point3DToGeoMsgPoint(end_p);
            corner_surf_marker.points.push_back(p1);
            corner_surf_marker.points.push_back(p2);
            corner_helper_marker.points.push_back(p2);
            end_p = node_ptr->position + node_ptr->surf_dirs.second * FARUtil::kVizRatio;
            p3 = FARUtil::Point3DToGeoMsgPoint(end_p);
            corner_surf_marker.points.push_back(p1);
            corner_surf_marker.points.push_back(p3);
            corner_helper_marker.points.push_back(p3);
        }
    };
    std::size_t idx = 0;
    const std::size_t graph_size = graph.size();
    nav_node_marker.points.resize(graph_size);
    for (const auto& nav_node_ptr : graph) {
        if (nav_node_ptr == NULL) {
            // DEBUG
            // ROS_WARN("Viz: graph includes NULL nodes");
            continue;
        }
        const geometry_msgs::Point cpoint = FARUtil::Point3DToGeoMsgPoint(nav_node_ptr->position);
        nav_node_marker.points[idx] = cpoint;
        if (!nav_node_ptr->is_finalized) {
            unfinal_node_marker.points.push_back(cpoint);
        }
        if (nav_node_ptr->is_navpoint) {
            internav_node_marker.points.push_back(cpoint);
        }
        if (nav_node_ptr->is_near_nodes) {
            near_node_marker.points.push_back(cpoint);
        }
        if (nav_node_ptr->is_covered) {
            covered_node_marker.points.push_back(cpoint);
        }
        if (nav_node_ptr->is_frontier) {
            frontier_node_marker.points.push_back(cpoint);
        }
        if (nav_node_ptr->is_boundary) {
            boundary_node_marker.points.push_back(cpoint);
        }
        Draw_Edge(nav_node_ptr);
        Draw_Surf_Dir(nav_node_ptr);
        Draw_Contour_Align(nav_node_ptr);
        idx ++;    
    } 
    nav_node_marker.points.resize(idx);
    graph_marker_array.markers.push_back(nav_node_marker);
    graph_marker_array.markers.push_back(unfinal_node_marker);
    graph_marker_array.markers.push_back(near_node_marker);
    graph_marker_array.markers.push_back(covered_node_marker);
    graph_marker_array.markers.push_back(frontier_node_marker);
    graph_marker_array.markers.push_back(internav_node_marker);
    graph_marker_array.markers.push_back(boundary_node_marker);
    graph_marker_array.markers.push_back(edge_marker);
    graph_marker_array.markers.push_back(visual_edge_marker);
    graph_marker_array.markers.push_back(free_edge_marker);
    graph_marker_array.markers.push_back(goal_edge_marker);
    graph_marker_array.markers.push_back(contour_edge_marker);
    graph_marker_array.markers.push_back(boundary_edge_marker);
    graph_marker_array.markers.push_back(odom_edge_marker);
    graph_marker_array.markers.push_back(traj_edge_marker);
    graph_marker_array.markers.push_back(corner_surf_marker);
    graph_marker_array.markers.push_back(corner_helper_marker);
    graph_marker_array.markers.push_back(contour_align_marker);
    viz_graph_pub_.publish(graph_marker_array);
}

void DPVisualizer::VizMapGrids(const PointStack& neighbor_centers, const PointStack& occupancy_centers,
                               const float& ceil_length, const float& ceil_height)
{
    MarkerArray map_grid_marker_array;
    Marker neighbor_marker, occupancy_marker;
    neighbor_marker.type = Marker::CUBE_LIST;
    occupancy_marker.type = Marker::CUBE_LIST;
    this->SetMarker(VizColor::GREEN, "neighbor_grids",  ceil_length / FARUtil::kVizRatio, 0.3f,  neighbor_marker);
    this->SetMarker(VizColor::RED,   "occupancy_grids", ceil_length / FARUtil::kVizRatio, 0.2f, occupancy_marker);
    neighbor_marker.scale.z = occupancy_marker.scale.z = ceil_height;
    const std::size_t N1 = neighbor_centers.size();
    const std::size_t N2 = occupancy_centers.size();
    neighbor_marker.points.resize(N1), occupancy_marker.points.resize(N2);
    for (std::size_t i=0; i<N1; i++) {
        geometry_msgs::Point p = FARUtil::Point3DToGeoMsgPoint(neighbor_centers[i]);
        neighbor_marker.points[i] = p;
    }
    for (std::size_t i=0; i<N2; i++) {
        geometry_msgs::Point p = FARUtil::Point3DToGeoMsgPoint(occupancy_centers[i]);
        occupancy_marker.points[i] = p;
    }
    map_grid_marker_array.markers.push_back(neighbor_marker);
    map_grid_marker_array.markers.push_back(occupancy_marker);
    viz_map_pub_.publish(map_grid_marker_array);
}

void DPVisualizer::SetMarker(const VizColor& color, 
                             const std::string& ns,
                             const float& scale,
                             const float& alpha,  
                             Marker& scan_marker, 
                             const float& scale_ratio) 
{
    scan_marker.header.frame_id = FARUtil::worldFrameId;
    scan_marker.header.stamp = ros::Time::now();
    scan_marker.id = 0;
    scan_marker.ns = ns;
    scan_marker.action = Marker::ADD;
    scan_marker.scale.x = scan_marker.scale.y = scan_marker.scale.z = scale * scale_ratio;
    scan_marker.pose.orientation.x = 0.0;
    scan_marker.pose.orientation.y = 0.0;
    scan_marker.pose.orientation.z = 0.0;
    scan_marker.pose.orientation.w = 1.0;
    scan_marker.pose.position.x = 0.0;
    scan_marker.pose.position.y = 0.0;
    scan_marker.pose.position.z = 0.0;
    DPVisualizer::SetColor(color, alpha, scan_marker);
}

void DPVisualizer::VizPointCloud(const ros::Publisher& viz_pub, 
                                 const PointCloudPtr& pc) 
{
    sensor_msgs::PointCloud2 msg_pc;
    pcl::toROSMsg(*pc, msg_pc);
    msg_pc.header.frame_id = FARUtil::worldFrameId;
    msg_pc.header.stamp = ros::Time::now();
    viz_pub.publish(msg_pc);
}

void DPVisualizer::SetColor(const VizColor& color, 
                            const float& alpha, 
                            Marker& scan_marker)
{
    std_msgs::ColorRGBA c;
    c.a = alpha;
    if (color == VizColor::RED) {
    c.r = 1.0f, c.g = c.b = 0.f;
    }
    else if (color == VizColor::ORANGE) {
    c.r = 1.0f, c.g = 0.45f, c.b = 0.1f;
    }
    else if (color == VizColor::BLACK) {
    c.r = c.g = c.b = 0.1f;
    }
    else if (color == VizColor::YELLOW) {
    c.r = c.g = 0.9f, c.b = 0.1;
    }
    else if (color == VizColor::BLUE) {
    c.b = 1.0f, c.r = 0.1f, c.g = 0.1f;
    }
    else if (color == VizColor::GREEN) {
    c.g = 0.9f, c.r = c.b = 0.f;
    }
    else if (color == VizColor::EMERALD) {
    c.g = c.b = 0.9f, c.r = 0.f;
    }
    else if (color == VizColor::WHITE) {
    c.r = c.g = c.b = 0.9f;
    }
    else if (color == VizColor::MAGNA) {
    c.r = c.b = 0.9f, c.g = 0.f;
    }
    else if (color == VizColor::PURPLE) {
    c.r = c.b = 0.5f, c.g = 0.f;
    }
    scan_marker.color = c;
}

