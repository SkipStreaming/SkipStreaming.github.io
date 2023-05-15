![GitHub](https://img.shields.io/github/license/SkipStreaming/SkipStreaming.github.io?color=important) ![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-brightgreen) ![Dependencies](https://img.shields.io/badge/dependencies-FFmpeg%20%7C%20Puppeteer-ff69b4) ![GitHub repo size](https://img.shields.io/github/repo-size/SkipStreaming/SkipStreaming.github.io)



## 0. Table of Contents

* [Motivation](#1-motivation)
* [Solution](#2-solution)
* [Code](#3-code)
* [Dataset](#4-dataset)

## 1. Motivation

The seemingly easy-to-mark visually redundant clips (e.g., intros, outros, recaps, and commercial breaks) have long been existent in correlated videos (e.g., a series of TV episodes, shows, and documentaries). Although mainstreaming video content providers (VCPs) such as  Netflix, Amazon Prime Video, HBO Max have provided corresponding functionalities for helping users skip them, they  incur poor user experiences in practice. We list the specific undesired symptoms as follows.

* **Probabilistic skip on the same clip:** A same redundant clip appears in a series of TV episodes. It is automatically skipped during the web-based playback of Episode 15, but not for Episode 16.

<video src='https://github.com/SkipStreaming/SkipStreaming.github.io/raw/main/videos/probabilistic.mp4' controls="" width="100%"></video>


* **Abrupt skip:** A redundant clip is skipped but with an incorrect duration that users can perceive. In considerable (16%) cases, the deviation is longer than 5 seconds.

<video src='https://github.com/SkipStreaming/SkipStreaming.github.io/raw/main/videos/abrupt_netflix.mp4' controls="" width="100%"></video>


* **Circumscribed skip:** Only the redundant clips at the head or the tail of correlated videos are skipped.

<video src='https://github.com/SkipStreaming/SkipStreaming.github.io/raw/main/videos/circumscribed.mp4' controls="" width="100%"></video>


* **Crash on skip:** When a skip action occurs, the web-based video player crashes.

<video src='https://github.com/SkipStreaming/SkipStreaming.github.io/raw/main/videos/crash.mp4' controls="" width="100%"></video>


## 2. Solution

![design_arch](https://user-images.githubusercontent.com/97234359/236974939-b5a7917e-39f7-45a9-8b7c-df6b78372c88.png)

SkipStreaming is a high-performance and accurate visual redundancy detection system for correlated videos.

SkipStreaming is built based on the novel perspective of "scenes", which are the basic story units that compose a video. It extracts scene information via our specially-designed "audio-guided scene sketch" methodology, and selectively compares a small portion of video frames to quickly detect visual redundancy.

This repository hosts the code of SkipStreaming and the video dataset involved in our study.

## 3. Code

The key component of SkipStreaming is a stand-alone server module implemented with a total of 4K and 300 lines of C/C++ and Python code, which can be seamlessly integrated into mainstream web servers with minimal configurations.

The source code of SkipStreaming is released at [https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/SkipStreaming](https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/SkipStreaming)

## 4. Dataset

We provide the information (in particular the marker information) about our dataset videos collected from major VCPs.

The dataset is available at [https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/dataset](https://github.com/SkipStreaming/SkipStreaming.github.io/tree/main/dataset)

| VCP                | Directory      | Marker Collection Methods                                    |
| ------------------ | -------------- | ------------------------------------------------------------ |
| Amazon Prime Video | amazon_res     | We capture all video series URLs in different genres of Amazon Prime Video by selecting the URLs from `.av-hover-wrapper` elements. Afterwards, we navigate to each video series URL and extract the `asin` property for each episode, which is then fed into Amazon Prime Video's API (`https://atv-ps.amazon.com/cdp/catalog/GetPlaybackResources`) to obtain the marker information. The intro/recap/outro timestamps are in the `transitionTimecodes` of the responded json data. |
| Disney+            | disney_content | We directly invoke Disney+'s `StandardCollection` API to obtain the genre information. For each genre, we query its video series information through the `CuratedSet` API. Based on this, we invoke its `DmcSeriesBundle` API to query different seasons of each video series as well as the `DmcEpisodes` API to obtain video series episodes' information. Finally, we get the episode's meta information through the `DmcVideo` API, and the marker information is inside the `milestones` fields of the responded json data. |
| HBO Max            | hbo_content    | We directly invoke `express-content` APIs to get the unique ID of each episode for each video series. Then, we invoke the `comet.api.hbo.com/content` API to obtain episodes' meta information. The marker information resides in the `annotations` fields of the responded json data. |
| Hulu               | hulu_play      | We first navigate to the Hulu's [home page](https://www.hulu.com/hub/tv/) to obtain the genre information of Hulu's library. For each available genre, we retrieve video series information using Hulu's `view_hubs` API. Then, we get the season information for each video series through the `series` API. Finally, we get the meta information of each episode in the video series through Hulu's `playlist` API. The marker information resides in the `markers` fields of the responded json data. |
| iQIYI              | iqiyi_dash     | We obtain the playlist of all available video series from iQIYI's [home page](https://www.iqiyi.com/list/%E7%94%B5%E8%A7%86%E5%89%A7). Then, we navigate to the playback page of each video series, and intercept its network requests to extract the episode information from its `dash` API. The marker information is in the responded `bt` and `et` fields. |
| Netflix            | netflix_meta   | We first get the URLs of Netflix's all [video genres](https://www.netflix.com/browse/genre/83). Then, we navigate to the genre pages to retrieve all video series URLs under each genre. Such URLs contain the unique ID for each video series, based on which we obtain video meta information from Netflix's `metadata` API. The responded data contain the maker information of each episode in the `skipMarkers` field. |
| Tencent Video      | tencent_video  | All video series are listed on Tencent Video's [video list page](https://v.qq.com/channel/tv/list?filter_params=ifeature%3D-1%26iarea%3D-1%26iyear%3D-1%26ipay%3D-1%26sort%3D79&page_id=channel_list_second_page). We obtain video series URLs from the list page, and visit each URL to get the unique IDs for each episode of the video series. These IDs are then passed into its `fcgi-bin/data?idlist` API to get the meta information. The marker information resides in the `head_time` and `tail_time` fields of the responded data. |
| Youku              | youku_appinfo  | All video series are also listed on Youku's [video list page](https://www.youku.com/category/show/c_97_u_1.html?theme=dark), and we also obtain all video series URLs. For each video series, we navigate to its playback page, and visit each episode. During this process, we monitor pages' network requests through Puppeteer pages' `on response` API, and get the returned data of Youku's `youku.play.ups.appinfo` API, where we can find the meta information of the video episode. The maker information is in the responded `point` field. |

