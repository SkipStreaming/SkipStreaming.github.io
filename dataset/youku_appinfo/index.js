const bluebird = require('bluebird');
const delay = require('delay');
// const pproxy = require('puppeteer-page-proxy');
const fs = require('fs');
const http = require("http");

const puppeteer = require('puppeteer-extra')
// const {goto} = require("puppeteer-bypass");
const StealthPlugin = require('puppeteer-extra-plugin-stealth')
puppeteer.use(StealthPlugin())

var ipPool = [];

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.iq.com";

const PAGE_SCROLL_DELAY = 500;

let genreCount = 0;

let showCount = 0;

async function getList(browser, show, page){
    let url = show.url;

    
    try {
        // await pproxy(page, `http://${ipPool[Math.floor(Math.random() * ipPool.length)]}`);
        await page.goto(url, {waitUntil: 'load', timeout: 20000});
    } catch (error) {
        console.log(error);
    }

    try {
        await page.waitForSelector('.new-box-anthology-items', {timeout: 5000});
    } catch (error) {
        console.log(error);

        let warning = await page.$$('.warnning-text');
        if (warning.length == 0) {
            await delay(2500);
        } else {
            await passValidate(page);
            await delay(10000);
        }
    }

    try {
        await page.waitForSelector('.new-box-anthology-items', {timeout: 5000});
    } catch (error) {
        console.log(error);

        let warning = await page.$$('.warnning-text');
        if (warning.length == 0) {
            await delay(2500);
        } else {
            await passValidate(page);
            await delay(10000);
        }
    }
    
    try {
        await page.waitForSelector('.new-box-anthology-items', {timeout: 5000});
    } catch (error) {
        console.log(error);
        return;
    }
    
    // var links = await page.evaluate(() => {
    //     var tabs = document.getElementsByClassName('top-wrap')[0];
    //     var links = [];
    //     for (var c of tabs.children) {
    //         c.click();

    //         var elements = document.querySelectorAll('.anthology-content > a');
    //         for (var e of elements) {
    //             links.push(e.getAttribute('href'));
    //         }
    //     }

    //     return links;
    // });

    var tabs = await page.$$('.new-box-anthology-tags.noflex > a');
    var links = [];

    if (tabs.length == 0) {
        var lks = await page.evaluate(() => {
            var links = [];
            var elements = document.querySelectorAll('.new-box-anthology-items > a');
            for (var e of elements) {
                links.push(e.getAttribute('href'));
            }
            return links;
        });

        links = links.concat(lks);
    } else {
        for (var tab of tabs) {
            await tab.click();
            await delay(1000);
            // var btns = await page.$$('.anthology-content > a');
            var lks = await page.evaluate(() => {
                var links = [];
                var elements = document.querySelectorAll('.new-box-anthology-items > a');
                for (var e of elements) {
                    links.push(e.getAttribute('href'));
                }
                return links;
            });
    
            links = links.concat(lks);
        }
    }

    // await delay(50000);
    page.on('response', async (response) => {
        var url =response.request().url();

        if (url.indexOf('youku.play.ups.appinfo') >= 0) {
            var data = await response.text();
            data = data.trim();
            if (data.indexOf('mtopjsonp1(') == 0) {
                data = data.replace('mtopjsonp1(', '');
                if (data[data.length - 1] === ')') {
                    data = data.substr(0, data.length - 1);  
                }
            }
            
            try {
                var jsonData = JSON.parse(data);

                let filename = `./data/${jsonData.data.data.show.title}_${('000' + jsonData.data.data.show.stage).substr(-3,3)}.json`;
                await fs.writeFileSync(filename, JSON.stringify(jsonData, null, 2));
            } catch (error) {
                console.log(error);
            }
        }
    }); 
    for (let index = 0; index < links.length; index++) {
        let lk = links[index];
        let filename = `./data/${show.name}_${('000' + (index + 1)).substr(-3,3)}.json`;
        if (fs.existsSync(filename)) {
            continue;
        }
        try {
            await page.goto(lk, {waitUntil: 'load', timeout: 10000});
        } catch (error) {
            console.log(error);
        }

        let warning = await page.$$('.warnning-text');
        if (warning.length == 0) {
            await delay(2500);
        } else {
            await passValidate(page);
            index--;
            await delay(2500);
        }

    }

    await page.close();
}

// function updatePool() {
//     http.get("http://www.zdopen.com/ShortProxy/GetIP/?api=202112242323256048&akey=bdb021d547e8d9ee&timespan=0&type=1", resp => {
//         let data = "";
//         resp.on("data", function(chunk) {
//             data += chunk;
//         });
//         resp.on("end", () => {
//             ipPool = data.split('\r\n');
//             console.log(ipPool)
//         });
//         resp.on("error", err => {
//             console.log(err.message);
//         });
//     });
// }

// updatePool();
// setInterval(updatePool, 15000);


async function passValidate(page) {
    // return await page.evaluate(() => {
    //    let iframe = document.getElementById('alibaba-login-box');
    //    let doc = iframe.contentDocument;
    //    doc.querySelector('#username').value='Bob';
    //    doc.querySelector('#password').value='pass123';
    // });
    const slider = await page.$('.nc_iconfont.btn_slide',);
    let slider_bounding = await slider.boundingBox();
    const bar = await page.$('.nc-lang-cnt');
    let bar_bounding = await await bar.boundingBox();
    await page.mouse.move(slider_bounding.x + slider_bounding.width / 2, slider_bounding.y + slider_bounding.height / 2);
    await page.mouse.down();
    await page.mouse.move(slider_bounding.x + bar_bounding.width + 50, slider_bounding.y + slider_bounding.height / 2);
    await page.mouse.up();
}

async function run() {
    const viewport = { width: 1920, height: 1080 };
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
        await page.goto('https://www.youku.com/category/show/c_97_u_1.html?theme=dark', {waitUntil: 'load', timeout: 0});

        await delay(30000);

        const cookies = await page.cookies();
        await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    const count = async () => {
        return page.evaluate(() => {
           return document.querySelectorAll('.g-col').length;
        });
    }

    let showInfo = [];

    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfo = JSON.parse(info);
    } else {
        try {
            await page.goto('https://www.youku.com/category/show/c_97_u_1.html?theme=dark', {waitUntil: 'load', timeout: 15000});
        } catch (error) {
            console.log(error)
        }
    
        const scrollDown = async () => {
            page.evaluate(()=>{
                document.querySelector('.g-col:last-child').scrollIntoView({ behavior: 'smooth', block: 'end', inline: 'end' });
            });
        }
    
        let preCount = 0;
        let postCount = 0;
        let retry = 10;
        do {
            preCount = await count();
            await scrollDown();
            await delay(PAGE_SCROLL_DELAY);
            postCount = await count();
            // console.log(postCount);

            if (postCount <= preCount) {
                retry--;
            } else {
                retry = 10;
            }

        } while (retry > 0);
        await delay(PAGE_SCROLL_DELAY);
    
        const shows = await page.$$('.categorypack_yk_pack_v');
    
        for (element of shows) {
            var show = await element.$eval('a', node => {return {name:node.getAttribute('title'), url: node.getAttribute('href')}});
            if (show.url.startsWith('//')) {
                show.url = 'https:' + show.url;
            }
            showInfo.push(show);
        }
    
    
        fs.writeFileSync('showInfo.json', JSON.stringify(showInfo));
    }
    var files = await fs.readdirSync('./data');
    // console.log(files);

    var beginShowName = '';
    var shouldBegin = true;

    for (let show of showInfo) {

        if (!shouldBegin) {
            if (show.name == beginShowName) {
                shouldBegin = true;
            } else {
                continue;
            }
        }

        let page = await browser.newPage();
        await page.setViewport(viewport);
        await getList(browser,show,page).catch((err)=>{console.log(err)});
    }

    await browser.close();
}

run().catch((err)=>{console.log(err)});