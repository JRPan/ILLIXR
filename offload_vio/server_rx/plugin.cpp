#include "common/plugin.hpp"
#include "common/switchboard.hpp"
#include "common/data_format.hpp"
#include "common/phonebook.hpp"

#include <ecal/ecal.h>
#include <ecal/msg/protobuf/subscriber.h>

#include "vio_input.pb.h"

using namespace ILLIXR;

class server_reader : public plugin {
public:
	server_reader(std::string name_, phonebook* pb_)
		: plugin{name_, pb_}
		, sb{pb->lookup_impl<switchboard>()}
		, _m_imu_cam{sb->get_writer<imu_cam_type>("imu_cam")}
    { 
		eCAL::Initialize(0, NULL, "VIO Server Reader");
		subscriber = eCAL::protobuf::CSubscriber<vio_input_proto::IMUCamVec>("vio_input");
		subscriber.AddReceiveCallback(std::bind(&server_reader::ReceiveVioInput, this, std::placeholders::_2));
	}

private:
	void ReceiveVioInput(const vio_input_proto::IMUCamVec& vio_input) {	
		// Loop through all IMU values first then the cam frame	
		for (int i = 0; i < vio_input.imu_cam_data_size(); i++) {
			vio_input_proto::IMUCamData curr_data = vio_input.imu_cam_data(i);

			std::optional<cv::Mat> cam0 = std::nullopt;
			std::optional<cv::Mat> cam1 = std::nullopt;

			if (curr_data.rows() != -1 && curr_data.cols() != -1) {

				// Must do a deep copy of the received data (in the form of a string of bytes)
				auto img0_copy = std::make_shared<std::string>(std::string(curr_data.img0_data()));
				auto img1_copy = std::make_shared<std::string>(std::string(curr_data.img1_data()));

				cv::Mat img0(curr_data.rows(), curr_data.cols(), CV_8UC1, img0_copy->data());
				cv::Mat img1(curr_data.rows(), curr_data.cols(), CV_8UC1, img1_copy->data());

				cam0 = std::make_optional<cv::Mat>(img0);
				cam1 = std::make_optional<cv::Mat>(img1);
			}

			_m_imu_cam.put(_m_imu_cam.allocate<imu_cam_type>(
				imu_cam_type {
					time_point{std::chrono::nanoseconds{curr_data.timestamp()}},
					Eigen::Vector3f{curr_data.angular_vel().x(), curr_data.angular_vel().y(), curr_data.angular_vel().z()},
					Eigen::Vector3f{curr_data.linear_accel().x(), curr_data.linear_accel().y(), curr_data.linear_accel().z()},
					cam0,
					cam1
				}
			));	
		}
	}

    const std::shared_ptr<switchboard> sb;
	switchboard::writer<imu_cam_type> _m_imu_cam;

	eCAL::protobuf::CSubscriber<vio_input_proto::IMUCamVec> subscriber;
};

PLUGIN_MAIN(server_reader)