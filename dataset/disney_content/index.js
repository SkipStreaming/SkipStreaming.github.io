const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const delay = require('delay');
const fs = require('fs');

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.disneyplus.com/";
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
    } else {
        await page.goto(baseUrl, {waitUntil: 'load', timeout: 0});

        await delay(120000);

        const cookies = await page.cookies();
        await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    let showInfo = {};
    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfo = JSON.parse(info);
    } else {
        try {
            await page.goto('https://disney.content.edge.bamgrid.com/svc/content/Collection/StandardCollection/version/5.1/region/US/audience/false/maturity/1499/language/en/contentClass/contentType/slug/series', {waitUntil: 'load', timeout: 15000});
            let innerText = await page.evaluate(() =>  {
                return JSON.parse(document.querySelector("body").innerText); 
            });
            await fs.writeFileSync('genres.json', JSON.stringify(innerText, null, 2));

            for (var genre of innerText.data.Collection.containers) {
                var genreId = genre.set.refId;
                var apiPage = 1;
                while (true) {
                    try {
                        await page.goto(`https://disney.content.edge.bamgrid.com/svc/content/CuratedSet/version/5.1/region/US/audience/false/maturity/1499/language/en/setId/${genreId}/pageSize/30/page/${apiPage}`, {waitUntil: 'load', timeout: 15000});
                        let seriesInfo = await page.evaluate(() =>  {
                            return JSON.parse(document.querySelector("body").innerText); 
                        });
                        if (seriesInfo.data.CuratedSet.items.length <= 0) {
                            break;
                        }

                        for (var item of seriesInfo.data.CuratedSet.items) {
                            showInfo[item.text.title.full.series.default.content] = {contentId: item.contentId, encodedSeriesId: item.encodedSeriesId};
                        }
                    } catch (error) {
                        console.log(error)
                    }
                    apiPage += 1;
                }
            }

            await fs.writeFileSync('showInfo.json', JSON.stringify(showInfo));
        } catch (error) {
            console.log(error);
        }
    }
    
    for (var seriesName in showInfo) {
        try {
            await page.goto(`https://disney.content.edge.bamgrid.com/svc/content/DmcSeriesBundle/version/5.1/region/US/audience/false/maturity/1499/language/en-us/encodedSeriesId/${showInfo[seriesName].encodedSeriesId}`, {waitUntil: 'load', timeout: 15000});
            let data = await page.evaluate(() =>  {
                return JSON.parse(document.querySelector("body").innerText); 
            });

            seasons = data.data.DmcSeriesBundle.seasons.seasons;
            
            let episodeIds = [];

            for (var season of seasons) {
                var seasonId = season.seasonId;
                var apiPage = 1;
                while (true) {
                    try {
                        await page.goto(`https://disney.content.edge.bamgrid.com/svc/content/DmcEpisodes/version/5.1/region/US/audience/false/maturity/1499/language/en/seasonId/${seasonId}/pageSize/15/page/${apiPage}`, {waitUntil: 'load', timeout: 15000});
                        let seasonInfo = await page.evaluate(() =>  {
                            return JSON.parse(document.querySelector("body").innerText); 
                        });
                        if (seasonInfo.data.DmcEpisodes.videos.length <= 0) {
                            break;
                        }

                        for (var video of seasonInfo.data.DmcEpisodes.videos) {
                            episodeIds.push(video.contentId);
                        }
                    } catch (error) {
                        console.log(error)
                    }
                    apiPage += 1;
                }
            }

            for (var episodeId of episodeIds) {
                await page.goto(`https://disney.content.edge.bamgrid.com/svc/content/DmcVideo/version/3.3/region/US/audience/false/maturity/1499/language/en/contentId/${episodeId}`, {waitUntil: 'load', timeout: 15000});
                let episodeInfo = await page.evaluate(() =>  {
                    return JSON.parse(document.querySelector("body").innerText); 
                });

                let seasonSequenceNumber = episodeInfo.data.DmcVideo.video.seasonSequenceNumber;
                let episodeSequenceNumber = episodeInfo.data.DmcVideo.video.episodeSequenceNumber;
                
                let filename = `./data/${seriesName.replace(/[^a-z0-9]/gi, '-')}_${seasonSequenceNumber}_${episodeSequenceNumber}.json`;
                await fs.writeFileSync(filename, JSON.stringify(episodeInfo));
            }
        } catch (error) {
            console.log(error);
        }
    }
    
    await browser.close()
}

run().catch((err)=>{console.log(err)});