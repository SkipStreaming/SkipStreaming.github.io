# SkipStreaming: Pinpointing User-Perceived Redundancy in Correlated Web Video Streaming through the Lens of Scenes

![GitHub](https://img.shields.io/github/license/SkipStreaming/SkipStreaming.github.io?color=important) ![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-brightgreen) ![Dependencies](https://img.shields.io/badge/dependencies-FFmpeg%20%7C%20Puppeteer-ff69b4) ![GitHub repo size](https://img.shields.io/github/repo-size/SkipStreaming/SkipStreaming.github.io)



## 0. Table of Contents

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

| Directory      | VCP                | Marker Collection Methods                                    |
| -------------- | ------------------ | ------------------------------------------------------------ |
| amazon_res     | Amazon Prime Video | We capture all video series URLs in different genres of Amazon Prime Video by selecting the URLs from `.av-hover-wrapper` elements. Afterwards, we navigate to each video series URL and extract the `asin` property for each episode, which is then fed into Amazon Prime Video's API (`https://atv-ps.amazon.com/cdp/catalog/GetPlaybackResources`) to obtain the marker information. The intro/recap/outro timestamps are in the `transitionTimecodes` of the responded json data. |
| disney_content | Disney+            | We directly invoke Disney+'s `StandardCollection` API to obtain the genre information. For each genre, we query its video series information through the `CuratedSet` API. Based on this, we invoke its `DmcSeriesBundle` API to query different seasons of each video series as well as the `DmcEpisodes` API to obtain video series episodes' information. Finally, we get the episode's meta information through the `DmcVideo` API, and the marker information is inside the `milestones` fields of the responded json data. |
| hbo_content    | HBO Max            | We directly invoke `express-content` APIs to get the unique ID of each episode for each video series. Then, we invoke the `comet.api.hbo.com/content` API to obtain episodes' meta information. The marker information resides in the `annotations` fields of the responded json data. |
| hulu_play      | Hulu               | We first navigate to the Hulu's [home page](https://www.hulu.com/hub/tv/) to obtain the genre information of Hulu's library. For each available genre, we retrieve video series information using Hulu's `view_hubs` API. Then, we get the season information for each video series through the `series` API. Finally, we get the meta information of each episode in the video series through Hulu's `playlist` API. The marker information resides in the `markers` fields of the responded json data. |
| iqiyi_dash     | iQIYI              | We obtain the play list of all available video series from iQIYI's [home page](https://www.iqiyi.com/list/%E7%94%B5%E8%A7%86%E5%89%A7). Then, we navigate to the playback page of each video series, and intercept its network requests to extract the episode information from its `dash` API. The marker information is in the responded `bt` and `et` fields. |
| netflix_meta   | Netflix            | We first get the URLs of Netflix's all [video genres](https://www.netflix.com/browse/genre/83). Then, we navigate to the genre pages to retrieve all video series URLs under each genre. Such URLs contain the unique ID for each video series, based on which we obtain video meta information from Netflix's `metadata` API. The responded data contain the maker information of each episode in the `skipMarkers` field. |
| tencent_video  | Tencent Video      | All video series are listed in Tencent Video's [video list page](https://v.qq.com/channel/tv/list?filter_params=ifeature%3D-1%26iarea%3D-1%26iyear%3D-1%26ipay%3D-1%26sort%3D79&page_id=channel_list_second_page). We obtain video series URLs from the list page, and visit each URL to get the unique IDs for each episode of the video series. These IDs are then passed into its `fcgi-bin/data?idlist` API to get the meta information. The marker information resides in the `head_time` and `tail_time` fields of the responded data. |
| youku_appinfo  | Youku              | All video series are also listed in Youku's [video list page](https://www.youku.com/category/show/c_97_u_1.html?theme=dark), and we also obtain all video series URLs. For each video series, we navigate to its playback page, and visit its each episode. During this process, we monitor pages' network requests through Puppeteer pages' `on response` API, and get the returned data of Youku's `youku.play.ups.appinfo` API, where we can find the meta information of the video episode. The maker information is in the responded `point` field. |

