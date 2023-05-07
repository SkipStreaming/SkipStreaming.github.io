const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const sleep = require('delay');
const fs = require('fs');
const { delay } = require('bluebird');
const jsdom = require("jsdom");
const { JSDOM } = jsdom;

//Base URL - DO NOT CHANGE
const baseUrl = "https://v.qq.com/";
const viewport = { width: 2000 , height: 1200 };

const PAGE_SCROLL_DELAY = 300;

let genreCount = 0;

let showCount = 0;

async function run() {
    const browser = await puppeteer.launch({
        headless: false, //Set this to false, to enable screen mode and debug
        executablePath: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
        args: [`--window-size=${viewport.width},${viewport.height}`]
    });
    const page = await browser.newPage();
    page.setViewport(viewport);
    // dom element selectors
    

    let showInfoDict = {};
    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfoDict = JSON.parse(info);
    } else {
        try {
            await page.goto('https://v.qq.com/channel/tv/list?filter_params=ifeature%3D-1%26iarea%3D-1%26iyear%3D-1%26ipay%3D-1%26sort%3D79&page_id=channel_list_second_page', {waitUntil: 'load', timeout: 15000});
        } catch (error) {
            console.log(error)
        }
    
        const gather = async () => {
            var currentElements = await page.$$('.card-list-wrap > a');
            let ret = 0;
            for (let element of currentElements) {
                var url = await (await element.getProperty('href')).jsonValue();
                var title = (await element.$eval('.title', node => {return node.textContent})).trim();
                var subtitle = (await element.$eval('.sub-title', node => {return node.textContent})).trim();
                if (!showInfoDict.hasOwnProperty(url)) {
                    showInfoDict[url] = {title: title, subtitle: subtitle};
                    console.log(showInfoDict);
                    ret++;
                }
            }
            console.log(`${ret} series detected.`);
            return ret;
        }

        await delay(5000);
        const elem = await page.$('.list-page-wrap');
        const boundingBox = await elem.boundingBox();
    
        let new_count = 0;
        let retry_count = 3;
        do {
            await delay(PAGE_SCROLL_DELAY);
            new_count = await gather();
            await page.mouse.move(
                boundingBox.x + boundingBox.width / 2,
                boundingBox.y + boundingBox.height / 2
              );
            await page.mouse.wheel({deltaY: 600});
            if (new_count == 0) {
                retry_count--;
            } else {
                retry_count = 3;
            }
        } while (new_count > 0 || retry_count > 0);
    
        fs.writeFileSync('showInfo.json', JSON.stringify(showInfoDict));
        await page.close();
    }

    showInfo = []
    for (let key in showInfoDict) {
        showInfo.push({url: key, title: showInfoDict[key].title.replace(':', '：'), subtitle: showInfoDict[key].subtitle})
    }
    
    await bluebird.Promise.map(showInfo, (show)=>{
        return new bluebird.Promise(async(resolve) => {

            let filename = `./video_ids/${show.title}_${show.subtitle}.json`;

            if (!fs.existsSync(filename)) {
                let htmlResponse = await fetch(show.url).then(response=>response.text());
                // console.log(htmlResponse);
                try {
                    const dom = new JSDOM(htmlResponse, { runScripts: "dangerously" });
                    if (dom.window.__pinia) {
                        fs.writeFileSync(filename, JSON.stringify(dom.window.__pinia, null, 2));
                    }
                } catch (error) {
                    
                }
                // let scripts = dom.window.document.querySelectorAll('script');
                // for (let script of scripts) {
                //     if (script.textContent.startsWith('window.__pinia=')) {
                //         let jsonContent = script.textContent.replace('window.__pinia=', '')
                //         fs.writeFileSync(filename, JSON.stringify(jsonContent, ' '));
                //         break;
                //     }
                // }
                // console.log(dom.window.__pinia);
            }
            // await page.setViewport(viewport);
            // await getList(browser,show,page).catch((err)=>{console.log(err)});
            resolve();
        });
    },{concurrency:1}); //Concurrency is set to 10. Lower this value if you have problem loading multiple pages simultaneously or if you have lower internet speeds.

    var filenames = fs.readdirSync('./video_ids');
    for (let filename of filenames) {
        filename = filename.replace('.json', '');
        let title = filename.split('_')[0];
        let subtitle = filename.split('_')[1];

        if (!fs.existsSync(`./data/${title}`)) {
            fs.mkdirSync(`./data/${title}`);
        }

        let showInfoJson = fs.readFileSync(`./video_ids/${filename}.json`);
        let videoIds = JSON.parse(showInfoJson)['global']['coverInfo']['video_ids'];

        await bluebird.Promise.map(videoIds, (id)=>{
            return new bluebird.Promise(async(resolve) => {
                let finalFilename = `./data/${title}/${id}.json`;
                if (!fs.existsSync(finalFilename)) {
                    let page = await browser.newPage();
                    await page.setViewport(viewport);
                    await page.goto(`https://union.video.qq.com/fcgi-bin/data?idlist=${id}&otype=json&tid=1804`);
                    let episodeData = await page.evaluate(() =>  {
                        return JSON.parse(document.querySelector("body").innerText.replace('QZOutputJson={', '{').slice(0, -1)); 
                    });
                    await page.close();
                    fs.writeFileSync(finalFilename, JSON.stringify(episodeData, null, 2))
                }
                resolve();
            });
        },{concurrency:5});
    }

    await browser.close();
}

run().catch((err)=>{console.log(err)});