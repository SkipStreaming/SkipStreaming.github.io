const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const delay = require('delay');
const fs = require('fs');
const request = require('request');

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.hbomax.com/";
const viewport = { width: 1920, height: 1080 };

async function autoScroll(page){
    await page.evaluate(async () => {
        await new Promise((resolve, reject) => {
            var totalHeight = 0;
            var distance = 200;
            var timer = setInterval(() => {
                var scrollHeight = document.body.scrollHeight;
                window.scrollBy(0, distance);
                totalHeight += distance;

                if(totalHeight >= scrollHeight){
                    clearInterval(timer);
                    resolve();
                }
            }, 100);
        });
    });
}

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

        await delay(120000);

        const cookies = await page.cookies();
        // await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    let showInfo = [];
    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfo = JSON.parse(info);
    } else {
        process.exit(0);
    }
    
    for (var showPage of showInfo) {
        await page.goto(`https://comet.api.hbo.com/express-content/${showPage}?device-code=desktop&product-code=hboMax&api-version=v9.0&country-code=US&signed-in=true&profile-type=adult&content-space=hboMaxExperience&brand=HBO%20MAX&territory=HBO%20MAX%20DOMESTIC&editorial-country=US&navigation-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&upsell-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&playback-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&client-version=hadron_50.63&language=en-us`, {waitUntil: 'load', timeout: 15000});
        let seriesData = await page.evaluate(() =>  {
            return JSON.parse(document.querySelector("body").innerText); 
        });

        for (var item of seriesData) {
            try {
                if (item.id.endsWith(':type:episode')) {

                    let filenameEpisode = '';
                    let filenameVideo = '';

                    if (item.body.seasonNumber && item.body.numberInSeason) {
                        filenameEpisode = `./data/${item.body.seriesTitles.full.replace(/[^a-z0-9]/gi, '-')}_${item.body.seasonNumber}_${item.body.numberInSeason}_episode.json`;
                        filenameVideo = `./data/${item.body.seriesTitles.full.replace(/[^a-z0-9]/gi, '-')}_${item.body.seasonNumber}_${item.body.numberInSeason}_video.json`;    
                    } else if (item.body.numberInSeries) {
                        filenameEpisode = `./data/${item.body.seriesTitles.full.replace(/[^a-z0-9]/gi, '-')}_${0}_${item.body.numberInSeries}_episode.json`;
                        filenameVideo = `./data/${item.body.seriesTitles.full.replace(/[^a-z0-9]/gi, '-')}_${0}_${item.body.numberInSeries}_video.json`;    
                    } else {
                        throw new Error(`Unable to generate filename: ${item.id}\n`);
                    }

                    
                    if (fs.existsSync(filenameVideo)) {
                        continue;
                    }
    
                    var episodeId = item.body.references.viewable;
    
                    await page.goto(`https://comet.api.hbo.com/express-content/${episodeId}?device-code=desktop&product-code=hboMax&api-version=v9.0&country-code=US&signed-in=true&profile-type=adult&content-space=hboMaxExperience&brand=HBO%20MAX&territory=HBO%20MAX%20DOMESTIC&editorial-country=US&navigation-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&upsell-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&playback-channels=HBO%20MAX%20SUBSCRIPTION%7CHBO%20MAX%20FREE&client-version=hadron_50.63&language=en-us`, {waitUntil: 'load', timeout: 15000});
                    let episodeData = await page.evaluate(() =>  {
                        return JSON.parse(document.querySelector("body").innerText); 
                    });
    
                    await fs.writeFileSync(filenameEpisode, JSON.stringify(episodeData, null, 2));
    
                    let editId = '';
    
                    for (var it of episodeData) {
                        if (it.id === episodeId) {
                            editId = it.body.references.edits[0];
                            break;
                        }
                    }
    
                    for (var it of episodeData) {
                        if (it.id === editId) {
                            let videoId = it.body.references.video;

                            var options = {
                                'method': 'POST',
                                'url': 'https://comet.api.hbo.com/content',
                                'proxy': 'http://127.0.0.1:1080',
                                'headers': {
                                    'accept-language': 'en-us',
                                    'authorization': 'Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJ0aW1lc3RhbXAiOjE2NDA3MTE3MjA2MzYsImV4cGlyYXRpb24iOjE2NDA3MjYxMjA2MzYsInBheWxvYWQiOnsiaGlzdG9yaWNhbE1ldGFkYXRhIjp7Im9yaWdpbmFsSXNzdWVkVGltZXN0YW1wIjoxNjQwNzA4MjY4MDkxLCJvcmlnaW5hbEdyYW50VHlwZSI6InVzZXJfcmVmcmVzaF9wcm9maWxlIiwib3JpZ2luYWxWZXJzaW9uIjoyfSwidG9rZW5Qcm9wZXJ0eURhdGEiOnsiY2xpZW50SWQiOiI1ODViMDJjOC1kYmUxLTQzMmYtYjFiYi0xMWNmNjcwZmJlYjAiLCJkZXZpY2VTZXJpYWxOdW1iZXIiOiJmNzZiZDg2NS1hOGJjLTRjMzUtZWU1NC1hZjU5ZDRjZjYzOTAiLCJwZXJtaXNzaW9ucyI6WzUsNCw3LDhdLCJjb3VudHJ5Q29kZSI6IlVTIiwicGxhdGZvcm1UZW5hbnRDb2RlIjoiaGJvRGlyZWN0IiwicHJvZHVjdENvZGUiOiJoYm9NYXgiLCJkZXZpY2VDb2RlIjoiZGVza3RvcCIsInBsYXRmb3JtVHlwZSI6ImRlc2t0b3AiLCJzZXJ2aWNlQ29kZSI6IkhCT19NQVgiLCJjbGllbnREZXZpY2VEYXRhIjp7InBheW1lbnRQcm92aWRlckNvZGUiOiJibGFja21hcmtldCJ9LCJhY2NvdW50UHJvdmlkZXJDb2RlIjoiaHVybGV5IiwidXNlcklkIjoiNDMwNTNkY2EtZmVkNC00Y2QyLWFlZDktNTdiYjZlN2Y3MGNlIiwiaHVybGV5QWNjb3VudElkIjoiNDMwNTNkY2EtZmVkNC00Y2QyLWFlZDktNTdiYjZlN2Y3MGNlIiwiaHVybGV5UHJvZmlsZUlkIjoiNDMwNTNkY2EtZmVkNC00Y2QyLWFlZDktNTdiYjZlN2Y3MGNlIiwicGFyZW50YWxDb250cm9scyI6eyJ0diI6IlRWLU1BIiwibW92aWUiOiJOQy0xNyJ9LCJzdHJlYW1UcmFja2luZ0lkIjoiNDMwNTNkY2EtZmVkNC00Y2QyLWFlZDktNTdiYjZlN2Y3MGNlIiwicmVxdWlyZXNBc3NldEF1dGh6IjpmYWxzZSwiYWZmaWxpYXRlQ29kZSI6Imhib19tYXhfb3R0IiwiaG9tZVNlcnZpY2VzUGFydGl0aW9uIjoibGF0YW0ifSwiZXhwaXJhdGlvbk1ldGFkYXRhIjp7ImF1dGh6VGltZW91dE1zIjoxNDQwMDAwMCwiYXV0aG5UaW1lb3V0TXMiOjMxMTA0MDAwMDAwLCJhdXRoekV4cGlyYXRpb25VdGMiOjE2NDA3MjYxMjA2MzYsImF1dGhuRXhwaXJhdGlvblV0YyI6MTY3MTgxNTcyMDYzNn0sImN1cnJlbnRNZXRhZGF0YSI6eyJlbnZpcm9ubWVudCI6InByb2R1Y3Rpb24iLCJtYXJrZXQiOiJsYXRhbSIsInZlcnNpb24iOjIsIm5vbmNlIjoiZTAwZGVjNmItZjk5OC00ZTc2LTg3NzItZDI2NzhhYjUwZDJlIiwiaXNzdWVkVGltZXN0YW1wIjoxNjQwNzExNzIwNjM2fSwicGVybWlzc2lvbnMiOls1LDQsNyw4XSwidG9rZW5fdHlwZSI6ImFjY2VzcyIsImVudmlyb25tZW50IjoicHJvZHVjdGlvbiIsIm1hcmtldCI6ImxhdGFtIiwidmVyc2lvbiI6Mn19.VvPpc1XFC6P3c6l_LLJKEDWDNZohDds1dVhcGwrt_JA',
                                    'content-type': 'application/json'
                                },
                                body: JSON.stringify([
                                    {
                                        "id": videoId,
                                        "headers": {
                                            "x-hbo-video-features": "server-stitched-playlist,mlp",
                                            "x-hbo-video-encodes": "H264|DASH|WDV"
                                        }
                                    }
                                ])

                            };
                            request(options, function (error, response) {
                                try {
                                    if (error) console.log(error);
                                    fs.writeFileSync(filenameVideo, JSON.stringify(JSON.parse(response.body), null, 2));
                                } catch (error) {
                                    console.log(error);
                                }
                                // console.log(response.body);
                            });

                            await delay(100);
                            break;
                        }
                    }
                }
            } catch (error) {
                console.log(error);
            }
        }
    }
    
    await browser.close()
}

run().catch((err)=>{console.log(err)});