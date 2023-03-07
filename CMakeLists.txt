cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)

project(eswifi_udp_poll_demo VERSION "2.0.0")

target_sources(app PRIVATE main.c)
