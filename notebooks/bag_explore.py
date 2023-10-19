import os
from pathlib import Path
import pytest
from rcl_interfaces.msg import Log
from rclpy.serialization import deserialize_message
import rosbag2_py
from rosidl_runtime_py.utilities import get_message
from std_msgs.msg import String

import px4_msgs
import plotly.express as px
import pandas as pd


def get_rosbag_options(path, storage_id, serialization_format='cdr'):
    storage_options = rosbag2_py.StorageOptions(
        uri=path, storage_id=storage_id)

    converter_options = rosbag2_py.ConverterOptions(
        input_serialization_format=serialization_format,
        output_serialization_format=serialization_format)

    return storage_options, converter_options


storage_id = 'sqlite3'
PLOT_GPS = True

if __name__ == '__main__':
    PANDA_MSGS = []
    LAPTOP_MSGS = []
    N = 5

    bag_path = "/home/ernie/thesis/bags/pi/rosbag2_2023_10_17-14_04_35"

    info = rosbag2_py.Info()
    metadata = info.read_metadata(bag_path, storage_id)
    panda_start_timestamp = metadata.starting_time.timestamp()

    storage_options, converter_options = get_rosbag_options(
        bag_path, storage_id)

    reader = rosbag2_py.SequentialCompressionReader()
    reader.open(storage_options, converter_options)

    topic_types = reader.get_all_topics_and_types()

    # Create a map for quicker lookup
    type_map = {
        topic_types[i].name: topic_types[i].type for i in range(len(topic_types))}

    # Set filter for topic of string type
    storage_filter = rosbag2_py.StorageFilter(
        topics=['/fmu/out/vehicle_gps_position'])
    if not PLOT_GPS:
        # TODO: add this
        # , '/fmu/out/timesync_status'
        pass
    reader.set_filter(storage_filter)

    msg_counter = 0
    while reader.has_next():
        (topic, data, t) = reader.read_next()
        msg_type = get_message(type_map[topic])
        msg = deserialize_message(data, msg_type)

        # print(msg)
        # print()
        PANDA_MSGS.append(msg)

        # assert isinstance(msg, String)
        # assert msg.data == f'Hello, world! {msg_counter}'

        # msg_counter += 1
        # if msg_counter == N*2:
        #     break

    if PLOT_GPS:
        msg = PANDA_MSGS[0]
        print(msg.lat * 1e-7)
        print(msg.lon * 1e-7)

        # create df from panda msgs
        df = pd.DataFrame(columns=['ID', 'Lat', 'Long'])
        lats = [msg.lat * 1e-7 for msg in PANDA_MSGS]
        longs = [msg.lon * 1e-7 for msg in PANDA_MSGS]
        df['ID'] = range(len(lats))
        df['Lat'] = lats
        df['Long'] = longs
        print(df)

        color_scale = [(0, 'orange'), (1, 'red')]

        fig = px.scatter_mapbox(df,
                                lat="Lat",
                                lon="Long",
                                hover_name="ID",
                                color_discrete_sequence=["red"],
                                zoom=16,
                                height=800,
                                width=800)
        # adding second trace to the figure
        # fig2 = px.line_mapbox(df, lat="lat", lon="lng", zoom=8)
        # fig.add_trace(fig2.data[0])  # adds the line trace to the first figure
        fig.update_layout(mapbox_style="open-street-map")
        fig.update_layout(margin={"r": 0, "t": 0, "l": 0, "b": 0})
        fig.show()
        exit()

    bag_path = "/home/ernie/thesis/bags/latest-niceish-very-far"
    storage_options, converter_options = get_rosbag_options(
        bag_path, storage_id)

    metadata = info.read_metadata(bag_path, storage_id)
    laptop_start_timestamp = metadata.starting_time.timestamp()

    reader = rosbag2_py.SequentialCompressionReader()
    reader.open(storage_options, converter_options)

    topic_types = reader.get_all_topics_and_types()

    # Create a map for quicker lookup
    type_map = {
        topic_types[i].name: topic_types[i].type for i in range(len(topic_types))}

    print(type_map)
    print()

    # Set filter for topic of string type
    storage_filter = rosbag2_py.StorageFilter(
        topics=['/fmu/out/vehicle_gps_position', '/fmu/out/timesync_status'])
    reader.set_filter(storage_filter)

    msg_counter = 0
    while reader.has_next():
        (topic, data, t) = reader.read_next()
        msg_type = get_message(type_map[topic])
        msg = deserialize_message(data, msg_type)

        print(msg)
        print()
        LAPTOP_MSGS.append(msg)

        # assert isinstance(msg, String)
        # assert msg.data == f'Hello, world! {msg_counter}'

        msg_counter += 1
        if msg_counter == N*2:
            break

    print([x.timestamp for x in PANDA_MSGS])

    timesync_panda = None
    # find timesync msg
    for msg in PANDA_MSGS:
        if isinstance(msg, px4_msgs.msg.TimesyncStatus):
            timesync_panda = msg
            break
    timesync_laptop = None
    # find timesync msg
    for msg in LAPTOP_MSGS:
        if isinstance(msg, px4_msgs.msg.TimesyncStatus):
            timesync_laptop = msg
            break
    for i in range(N*2):
        if not isinstance(PANDA_MSGS[i], px4_msgs.msg.SensorGps):
            continue
        if not isinstance(LAPTOP_MSGS[i], px4_msgs.msg.SensorGps):
            continue
        gps_panda = PANDA_MSGS[i]
        gps_laptop = LAPTOP_MSGS[i]

        print()
        print(gps_panda)
        print(timesync_panda)
        print()
        print(gps_laptop)
        print(timesync_laptop)

        print()
        gps_panda.timestamp -= timesync_panda.observed_offset
        print(gps_panda.timestamp / 1e6)
        print(gps_laptop.timestamp / 1e6)
        print((gps_panda.timestamp / 1e6 - gps_laptop.timestamp / 1e6) / 60)

        break

        print()
        print(gps_panda.timestamp)
        print(gps_laptop.timestamp - timesync_laptop.observed_offset)

        # diff between
        print()
        diff = gps_panda.timestamp - \
            (gps_laptop.timestamp - timesync_laptop.observed_offset)
        diff /= 1e6
        print(diff)
        print(panda_start_timestamp - laptop_start_timestamp)
        break
