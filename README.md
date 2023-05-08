# SkipStreaming: Pinpointing User-Perceived Redundancy in Correlated Web Video Streaming through the Lens of Scenes

![GitHub](https://img.shields.io/github/license/SkipStreaming/SkipStreaming.github.io?color=important)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-brightgreen)
![GitHub repo size](https://img.shields.io/github/repo-size/SkipStreaming/SkipStreaming.github.io)



## 0. Table of Content

* [Introduction](#1-Introduction)
* [Code](#2-Code)
* [Dataset](#3-Dataset)

## 1. Introduction

SkipStreaming is a high-performance and accurate visual redundancy detection system for correlated videos (e.g., a series of TV episodes, shows, and documentaries).

SkipStreaming is built based on the novel perspective of "scenes", which are the basic story units that compose a video. It extracts scene information via our specially-designed "audio-guided scene sketch" methodology, and selectively compares a small portion of video frames to quickly detect visual redundancy.

This repository hosts the code of SkipStreaming and the video dataset involved in our study.

## 2. Code

The key component of SkipStreaming is a stand-alone server module implemented with a total of 4K and 300 lines of C/C++ and Python code, which can be seamlessly integrated into mainstream web servers with minimal configurations.

The source code of SkipStreaming is released at [https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/SkipStreaming](https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/SkipStreaming)

## 3. Dataset

We provide the information (in particular the marker information) about our dataset videos collected from major video content providers (e.g., Netflix, Amazon Prime Video, HBO Max).

The dataset is available at [https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/dataset](https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/dataset)

