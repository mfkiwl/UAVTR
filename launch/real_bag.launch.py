import numpy as np
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution
import os

Q = [1.314119854794893e-05, -0.0010005848113152764, 0.00039714285724196113, -4.376696169173706e-05, 6.768401142184756e-05, -1.1919112919677553e-05, -1.8944994373367454e-05, -9.582839271219149e-05, -0.0001341873301673638, 0.00019985248082600416, 0.00019636725818867546, 0.00019755766907509292, -6.492534224793265e-05, 1.8853392870351242e-05, -0.0010005848113152764, 0.07618934428233949, -0.030265775885232344, 0.003756238135495654, -0.005204561912902614, 0.0009263316686388221, 0.0013920981461207269, 0.007279472344020756, 0.010516278972146442, -0.015296270279283744, -0.014991425013115742, -0.015289439541828818, 0.0048951907901601125, -0.0014829398703750667, 0.00039714285724196113, -0.030265775885232344, 0.012218240999907802, -0.003946071668256014, 0.0020944698804307694, 0.0003784404292556269, -0.0001501434113812713, -0.0027907037419297974, -0.0062863350160735475, 0.006762000462206191, 0.006638280593061481, 0.007304010630554945, -0.0013205887527499207, 3.858956884203739e-05, -4.376696169173706e-05, 0.003756238135495654, -0.003946071668256014, 0.05526053695889853, -0.011291223604521542, 0.0176398587601795, -0.004542019119068548, -0.001893296052397691, 0.03244229243598404, -0.006814911864383088, 0.002295505087314671, -0.03633619670814229, -0.00023337817466384265, -0.020751881678352013, 6.768401142184756e-05, -0.005204561912902614, 0.0020944698804307694, -0.011291223604521542, 0.02931759768820403, -0.0050859873201332065, -0.00012517897785548735, -0.00028248268507743334, -0.022323746027066466, 0.006896073937324485, 0.001595692979638231, 0.0047225877479757416, -0.021915058140415708, 0.01428566268915521, -1.1919112919677553e-05, 0.0009263316686388221, 0.0003784404292556269, 0.0176398587601795, -0.0050859873201332065, 0.041927996103169614, 0.002158726915346784, -0.0009433615422732077, -0.014307690919990694, -0.005681757729608296, 0.02852764788084052, -0.019713929280997217, 0.0010689419344616204, -0.033065633774814265, -1.8944994373367454e-05, 0.0013920981461207269, -0.0001501434113812713, -0.004542019119068548, -0.00012517897785548735, 0.002158726915346784, 0.0008675221102249851, 0.0003219430506327591, -0.004047496783904383, 0.001042496561925873, 0.001560011916158765, 0.001954578347581331, 0.0013281652561750496, -
     0.0016685593609775443, -9.582839271219149e-05, 0.007279472344020756, -0.0027907037419297974, -0.001893296052397691, -0.00028248268507743334, -0.0009433615422732077, 0.0003219430506327591, 0.0008039714032473805, 0.0004400364509027877, -0.0008580146253704132, -0.0018275479083328114, 0.00021428813520473058, 0.0007702982390550453, 0.0007680289937804904, -0.0001341873301673638, 0.010516278972146442, -0.0062863350160735475, 0.03244229243598404, -0.022323746027066466, -0.014307690919990694, -0.004047496783904383, 0.0004400364509027877, 0.07164520012319903, -0.0031273374619571036, -0.005202041228014463, -0.005324463185108623, 0.011421485528768698, 0.0036676975396757352, 0.00019985248082600416, -0.015296270279283744, 0.006762000462206191, -0.006814911864383088, 0.006896073937324485, -0.005681757729608296, 0.001042496561925873, -0.0008580146253704132, -0.0031273374619571036, 0.03636582527009013, -0.00705526743562396, 0.00037647006886462755, 0.007393087385012548, 0.0014457899566499993, 0.00019636725818867546, -0.014991425013115742, 0.006638280593061481, 0.002295505087314671, 0.001595692979638231, 0.02852764788084052, 0.001560011916158765, -0.0018275479083328114, -0.005202041228014463, -0.00705526743562396, 0.055517849113261276, -0.0009473163323801689, -0.005530500055860457, -0.004715228831639893, 0.00019755766907509292, -0.015289439541828818, 0.007304010630554945, -0.03633619670814229, 0.0047225877479757416, -0.019713929280997217, 0.001954578347581331, 0.00021428813520473058, -0.005324463185108623, 0.00037647006886462755, -0.0009473163323801689, 0.03532469813132734, -0.0021348117181617526, 0.019157849594863458, -6.492534224793265e-05, 0.0048951907901601125, -0.0013205887527499207, -0.0002333781746638426, -0.021915058140415708, 0.0010689419344616213, 0.0013281652561750496, 0.0007702982390550453, 0.011421485528768698, 0.00739308738501255, -0.005530500055860456, -0.002134811718161752, 0.02416537704403732, -0.009885880730604807, 1.8853392870351242e-05, -0.0014829398703750667, 3.858956884203739e-05, -0.020751881678352013, 0.014285662689155211, -0.03306563377481427, -0.0016685593609775445, 0.0007680289937804904, 0.0036676975396757357, 0.00144578995665, -0.004715228831639892, 0.019157849594863455, -0.009885880730604807, 0.03949202658848555]
Q[14*6+6] = Q[14*6+6]*10
Q[14*6+7] = Q[14*6+7]*10 
pos_R = [25.13819427540937, -6.7611250286672, -
         6.7611250286672, 11.440427146986359]
vel_R = [0.5570656178441566, -0.016321310276384324, 0.036256032351717184, -0.016321310276384324,
         0.4492023783736639, -0.0012988113834949844, 0.036256032351717184, -0.0012988113834949827, 0.042578026212086395]
acc_R = [8.508143595890573, -0.016884870145245368, 0.4322423989673038, -0.016884870145245368,
         11.219244184596246, -1.5755884699833724, 0.4322423989673038, -1.5755884699833724, 5.243079208665659]

# Q = [2.27157453e-05, 1.07853528e-04, 9.18906052e-03,
#      8.21363025e-03, 9.95703086e-03, 1.83803188e-02,
#      1.21363025e-2, 1.21363025e-2,
#      6.69823597e-03, 6.26658635e-03, 7.09409103e-03,
#      1e-5, 1e-5, 1e-5]
# Qnp = np.diag(np.array(Q))
# Q = list(Qnp.reshape(-1))

# pos_R = [4.148242398539099, 0.0,
#          0.0, 5.702412298355506]
# vel_R = [0.16966872074959238, 0.0, 0.0,
#          0.0, 0.06049256508421064, 0.0,
#          0.0, 0.0, 0.13327101067543388]
# acc_R = [3.652282205366519, 0.0, 0.0,
#          0.0, 4.333955375627488, 0.0,
#          0.0, 0.0, 3.23400083776894456]


def launch_setup(context, *args, **kwargs):
    root_dir = os.path.dirname(
        os.path.dirname(os.path.realpath(__file__)))

    gain = 0.7
    magnetic_rejection = 0.0
    acceleration_rejection = 15.0
    recovery_trigger_period = 1
    params = ["--ros-args",
              "-p", f"gain:={gain}",
              "-p", f"magnetic_rejection:={magnetic_rejection}",
              "-p", f"acceleration_rejection:={acceleration_rejection}",
              "-p", f"recovery_trigger_period:={recovery_trigger_period}"]
    orientation_filter = ExecuteProcess(
        cmd=[f'{root_dir}/src/estimation/build/orientation_filter'] + params,
        output='screen',
    )

    tracking = ExecuteProcess(
        cmd=['./tracking_ros_node'],
        cwd=f'{root_dir}/src/detection/build',
        # prefix=['xterm  -e gdb -ex "run" --args'],
        output='screen'
    )

    uncompress = ExecuteProcess(
        cmd=['ros2', 'run', 'image_transport', 'republish', 'compressed', 'raw', '--ros-args', '-r',
             '/in/compressed:=/camera/color/image_raw/compressed', '-r', 'out:=/camera/color/image_raw'],
        output='screen'
    )

    WHICH = int(context.launch_configurations['which'])
    MODE = int(context.launch_configurations['mode'])
    bag_name = ""
    offset = -1
    BAG0_OFF = 130
    if WHICH == 0:
        bag_name = "./18_0/rosbag2_2023_10_18-12_24_19"
        offset = BAG0_OFF
    else:
        bag_name = "./latest_flight/rosbag2_2023_10_18-16_22_16"
        modes = [
            1500,  # going to the moon
            1900,  # 1942,
            2245
        ]
        offset = modes[MODE]

    play_bag_cmd = f'''ros2 bag play {bag_name} --start-offset {offset}'''
    play_bag = ExecuteProcess(
        cmd=play_bag_cmd.split(),
        cwd=f"{root_dir}/bags",
        output='screen'
    )

    run_name = bag_name.split('/')[-2]
    if offset != BAG0_OFF:
        run_name += f'_mode{MODE}'

    record_state = ExecuteProcess(
        cmd=["python3", "record_state.py", run_name],
        cwd=f"{root_dir}/scripts",
        output='screen'
    )

    baro_ref = 0.0
    if WHICH == 0:
        baro_ref = 25.94229507446289
    else:
        baro_ref = 7.0
    flow_err_threshold = 5.0
    estimation = ExecuteProcess(
        cmd=["./estimation_node", "--ros-args",
             "-p", f"baro_ground_ref:={baro_ref}",
             "-p", f"spatial_vel_flow_error:={flow_err_threshold}",
             "-p", f"flow_vel_rejection_perc:={10.0}",
             "-p", f"process_covariance:={Q}",
             "-p", f"pos_R:={pos_R}",
             "-p", f"vel_R:={vel_R}",
             "-p", f"acc_R:={acc_R}"],
        cwd=f'{root_dir}/src/estimation/build',
        # prefix=['xterm  -e gdb -ex "run" --args'],
        output='screen'
    )
    # wrap into timer to launch 2 seconds after everything
    est_timer = TimerAction(
        period=1.0,
        actions=[estimation]
    )

    return [
        play_bag,
        tracking,
        est_timer,
        uncompress,
        orientation_filter,
        record_state
    ]


def generate_launch_description():
    # Declare the command line arguments
    which_arg = DeclareLaunchArgument(
        'which',
        default_value='0',  # 0 or 1
        description='Which argument'
    )

    mode_arg = DeclareLaunchArgument(
        'mode',
        default_value='0',  # 0, 1, 2
        description='Mode argument'
    )

    evaluate_args = OpaqueFunction(function=launch_setup)

    return LaunchDescription([
        which_arg,
        mode_arg,
        evaluate_args
    ])
