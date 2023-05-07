const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const delay = require('delay');
const fs = require('fs');
const request = require('request');

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.hulu.com/hub/tv/";
const viewport = { width: 1920, height: 1080 };

async function run() {
    const browser = await puppeteer.launch({
        headless: false, //Set this to false, to enable screen mode and debug
        executablePath: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
        args: [`--window-size=${viewport.width},${viewport.height}`]
    });
    const page = await browser.newPage();
    page.setViewport(viewport);
    // dom element selectors
    

    const cookieFile = 'cookies.json';

    if (fs.existsSync(cookieFile)) {
        const cookiesString = await fs.readFileSync(cookieFile);
        const cookies = JSON.parse(cookiesString);
        await page.setCookie(...cookies);
        await page.goto(baseUrl, {waitUntil: 'load', timeout: 0});
    } else {
        await page.goto(baseUrl, {waitUntil: 'load', timeout: 0});

        await delay(45000);

        const cookies = await page.cookies();
        await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfo = JSON.parse(info);
    } else {

        await page.waitForSelector('.ModalHeader__line-2');
        await page.click('.ModalHeader__line-2');
    
        let labels = [];
        let genreCollection = [];
        let canStop = false;

        while (!canStop) {
            try {
                canStop = true;
                await delay(5000);
                let genrePanel = await page.$$('#__next > div.LevelOne.cu-levelone > div.Hub > div > div:nth-child(1) > div > div > div.SliderV2.StandardSliderCollectionSimple__slider > div > ul > li');

                for (let panel of genrePanel) {

                    let label = await panel.$eval('.StandardTextTile__label', e => e.innerText);
                    if (labels.indexOf(label) >= 0) {
                        continue;
                    } else {
                        canStop = false;
                        labels.push(label);
                    }

                    await panel.click();
                    await delay(4000);

                    let url = await page.evaluate(() => document.querySelector('#LevelTwo__scroll-area > div > div > div.Container > div > div > div:nth-last-child(1) > div > div > div.CollectionHeader-module-CollectionHeader-D5um_ > div.CollectionHeader-module-CollectionHeader__view-all-wrapper-BBIJK > div > a').href);
                    console.log(url);
                    genreCollection.push(url);
                    await page.waitForSelector('.Button.Nav__link.cu-nav-link.cu-leveltwo-close.Button--high-emphasis');
                    await page.click('.Button.Nav__link.cu-nav-link.cu-leveltwo-close.Button--high-emphasis');
                    await delay(2000);
                }

                await page.waitForSelector('.SliderV2__button.SliderV2__button--next.SliderV2__button--next--ready', {timeout: 60000});
                await page.click('.SliderV2__button.SliderV2__button--next.SliderV2__button--next--ready');
            } catch (error) {
                console.log(error);
                break;
            }
        }

        let showInfo = {};

        for (let url of genreCollection) {
            let collection = url.slice(25)
            let apiUrl = `https://discover.hulu.com/content/v5/view_hubs/${collection.replace('collection','collections')}?schema=1&limit=9999`
            await page.goto(apiUrl, {waitUntil: 'load', timeout: 15000});
            let seriesData = await page.evaluate(() =>  {
                return JSON.parse(document.querySelector("body").innerText); 
            });

            for (let item of seriesData.items) {
                let seriesName = item.metrics_info.target_name;
                let id = item.id;
                showInfo[seriesName] = id;
            }
        }
        await fs.writeFileSync('showInfo.json', JSON.stringify(showInfo));
    }
    
    for (let seriesName in showInfo) {
        let season = 1;
        while (true) {
            await page.goto(`https://discover.hulu.com/content/v5/hubs/series/${showInfo[seriesName]}/season/${season}?limit=1999&schema=1&offset=0`, {waitUntil: 'load', timeout: 15000});
            let seasonData = await page.evaluate(() =>  {
                return JSON.parse(document.querySelector("body").innerText); 
            });

            let items = seasonData.items;
            if (items.length <= 0) {
                break;
            }

            for (let item of items) {

                let episode = item.number;
                let sea = item.season;
                let eab = item.personalization.eab;

                let filename = `./data/${seriesName.replace(/[^a-z0-9]/gi, '-')}_${season}_${episode}.json`;

                var options = {
                    'method': 'POST',
                    'url': 'https://play.hulu.com/v6/playlist',
                    'proxy': 'http://127.0.0.1:1080',
                    'headers': {
                        'Connection': 'keep-alive',
                        'Pragma': 'no-cache',
                        'Cache-Control': 'no-cache',
                        'X-HULU-USER-AGENT': 'site-player/308343',
                        'sec-ch-ua-mobile': '?0',
                        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/97.0.4692.71 Safari/537.36',
                        'sec-ch-ua': '" Not;A Brand";v="99", "Google Chrome";v="97", "Chromium";v="97"',
                        'sec-ch-ua-platform': '"Windows"',
                        'Content-type': 'application/json',
                        'Accept': '*/*',
                        'Origin': 'https://www.hulu.com',
                        'Sec-Fetch-Site': 'same-site',
                        'Sec-Fetch-Mode': 'cors',
                        'Sec-Fetch-Dest': 'empty',
                        'Referer': 'https://www.hulu.com/',
                        'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
                        'Cookie': 'YOUR COOKIE'
                      },
                    body: JSON.stringify({
                        "deejay_device_id": 190,
                        "version": 1,
                        "content_eab_id": eab,
                        "unencrypted": true,
                        "playback": {
                            "version": 2,
                            "video": {
                                "codecs": {
                                    "values": [
                                        {
                                            "type": "H264",
                                            "width": 1920,
                                            "height": 1080,
                                            "framerate": 60,
                                            "level": "4.2",
                                            "profile": "HIGH"
                                        }
                                    ],
                                    "selection_mode": "ONE"
                                }
                            },
                            "audio": {
                                "codecs": {
                                    "values": [
                                        {
                                            "type": "AAC"
                                        }
                                    ],
                                    "selection_mode": "ONE"
                                }
                            },
                            "drm": {
                                "values": [
                                    {
                                        "type": "WIDEVINE",
                                        "version": "MODULAR",
                                        "security_level": "L3"
                                    },
                                    {
                                        "type": "PLAYREADY",
                                        "version": "V2",
                                        "security_level": "SL2000"
                                    }
                                ],
                                "selection_mode": "ALL"
                            },
                            "manifest": {
                                "type": "DASH",
                                "https": true,
                                "multiple_cdns": true,
                                "patch_updates": true,
                                "hulu_types": true,
                                "live_dai": true,
                                "multiple_periods": false,
                                "xlink": false,
                                "secondary_audio": true,
                                "live_fragment_delay": 3
                            },
                            "segments": {
                                "values": [
                                    {
                                        "type": "FMP4",
                                        "encryption": {
                                            "mode": "CENC",
                                            "type": "CENC"
                                        },
                                        "https": true
                                    }
                                ],
                                "selection_mode": "ONE"
                            }
                        }
                    })

                };
                request(options, function (error, response) {
                    try {
                        if (error) console.log(error);
                        let data = JSON.parse(response.body);
                        data['series_name'] = seriesName;
                        data['season'] = sea.toString();
                        data['episode'] = episode.toString();
                        fs.writeFileSync(filename, JSON.stringify(data, null, 2));
                    } catch (error) {
                        console.log(error);
                    }
                    // console.log(response.body);
                });
                await delay(50);
            }
            season += 1;
        }
    }
    
    await delay(300000);
    await browser.close()
}

run().catch((err)=>{console.log(err)});