const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const delay = require('delay');
const fs = require('fs');
const axios = require('axios');

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.amazon.com/Amazon-Video/b/?&node=2858778011&ref=nav_custrec_signin&";
const viewport = { width: 1920, height: 1080 };

const PAGE_SCROLL_DELAY = 4000;

let genreCount = 0;

let showCount = 0;

async function getList(browser, show, page){
    let url = show.url;
    var itemList = [];
    // page.on('response', async (response) => {
    //     var url =response.request().url();

    //     if (url.indexOf('cdp/discovery/GetSections') >= 0) {
    //         try {
    //             var data = await response.text();
    //             // console.log(data);
    //             var jsonData = JSON.parse(data);
                
    //             itemList = jsonData.sections.bottom.collections.collectionList[0].items.itemList;
                
    //         } catch (error) {
    //             console.log(error);
    //         }
    //     }
    // }); 

    try {
        await page.goto(url, {waitUntil: 'load', timeout: 24000});
    } catch (error) {
        console.log(error)
    }

    // await delay(15000);

    const seasonSel = await page.$$('.dv-node-dp-seasons li');
    var seasonUrls = [];
    for (var sea of seasonSel) {
        let u = await sea.$eval('a', node => {return node.getAttribute('href')});
        u = 'https://www.amazon.com' + u;
        // console.log(u);
        seasonUrls.push(u);
    }

    if (seasonSel.length > 0) {

        for (var u of seasonUrls) {
            console.log(u);

            await page.goto(u, {waitUntil: 'load', timeout: 15000});

            const playBtns = await page.$$('.dv-episode-playback-title');
            var asins = [];
            for (var btn of playBtns) {
                let u = await btn.$eval('a', node => {return node.getAttribute('href')});
                var asin = u.split('/')[4];
                asins.push(asin);
            }

            for (var asin of asins) {

                await page.goto(`https://atv-ps.amazon.com/cdp/catalog/GetPlaybackResources?deviceID=ad00c6eb5bdefbcc28ef868dc43c289ff91b42373f7c4fae695b1b85&deviceTypeID=AOAGZA014O5RE&firmware=1&consumptionType=Streaming&desiredResources=PlaybackUrls%2CCuepointPlaylist%2CCatalogMetadata%2CSubtitleUrls%2CForcedNarratives%2CTrickplayUrls%2CTransitionTimecodes%2CPlaybackSettings%2CXRayMetadata&resourceUsage=CacheResources&videoMaterialType=Feature&titleDecorationScheme=primary-content&asin=${asin}`);
                let innerText = await page.evaluate(() =>  {
                    return JSON.parse(document.querySelector("body").innerText); 
                });
                try {
                    if (innerText.catalogMetadata.catalog.entityType === 'TV Show') {
                        var ancestors = innerText.catalogMetadata.family.tvAncestors;
                        var title = '', season = '';
                        for (var a of ancestors) {
                            if (a.catalog.type === 'SEASON') {
                                season = a.catalog.seasonNumber;
                            } else if (a.catalog.type === 'SHOW') {
                                title = a.catalog.title;
                            }
                        }

                        let filename = `./data/${title.replace(/[^a-z0-9]/gi, '-')}_${season}_${('000' + innerText.catalogMetadata.catalog.episodeNumber).substr(-3,3)}.json`;
                        await fs.writeFileSync(filename, JSON.stringify(innerText, null, 2));
                    }
                } catch (error) {
                    console.log(error)
                }
            }
        }
    } else {

        const playBtns = await page.$$('.dv-episode-playback-title');
        var asins = [];
        for (var btn of playBtns) {
            let u = await btn.$eval('a', node => {return node.getAttribute('href')});
            var asin = u.split('/')[4];
            asins.push(asin);
        }

        for (var asin of asins) {

            await page.goto(`https://atv-ps.amazon.com/cdp/catalog/GetPlaybackResources?deviceID=ad00c6eb5bdefbcc28ef868dc43c289ff91b42373f7c4fae695b1b85&deviceTypeID=AOAGZA014O5RE&firmware=1&consumptionType=Streaming&desiredResources=PlaybackUrls%2CCuepointPlaylist%2CCatalogMetadata%2CSubtitleUrls%2CForcedNarratives%2CTrickplayUrls%2CTransitionTimecodes%2CPlaybackSettings%2CXRayMetadata&resourceUsage=CacheResources&videoMaterialType=Feature&titleDecorationScheme=primary-content&asin=${asin}`);
            let innerText = await page.evaluate(() =>  {
                return JSON.parse(document.querySelector("body").innerText); 
            });
            try {
                if (innerText.catalogMetadata.catalog.entityType === 'TV Show') {
                    var ancestors = innerText.catalogMetadata.family.tvAncestors;
                    var title = '', season = '';
                    for (var a of ancestors) {
                        if (a.catalog.type === 'SEASON') {
                            season = a.catalog.seasonNumber;
                        } else if (a.catalog.type === 'SHOW') {
                            title = a.catalog.title;
                        }
                    }

                    let filename = `./data/${title.replace(/[^a-z0-9]/gi, '-')}_${season}_${('000' + innerText.catalogMetadata.catalog.episodeNumber).substr(-3,3)}.json`;
                    await fs.writeFileSync(filename, JSON.stringify(innerText, null, 2));
                }
            } catch (error) {
                console.log(error)
            }
        }

        // for (var item of itemList) {
        //     console.log(item.titleId);
        //     var asin = item.titleId;
        //     await page.goto(`https://atv-ps.amazon.com/cdp/catalog/GetPlaybackResources?deviceID=ad00c6eb5bdefbcc28ef868dc43c289ff91b42373f7c4fae695b1b85&deviceTypeID=AOAGZA014O5RE&firmware=1&consumptionType=Streaming&desiredResources=PlaybackUrls%2CCuepointPlaylist%2CCatalogMetadata%2CSubtitleUrls%2CForcedNarratives%2CTrickplayUrls%2CTransitionTimecodes%2CPlaybackSettings%2CXRayMetadata&resourceUsage=CacheResources&videoMaterialType=Feature&titleDecorationScheme=primary-content&asin=${asin}`);
        //     let innerText = await page.evaluate(() =>  {
        //         return JSON.parse(document.querySelector("body").innerText); 
        //     });
        //     try {
        //         if (innerText.catalogMetadata.catalog.entityType === 'TV Show') {
    
        //             let filename = `./data/${innerText.catalogMetadata.catalog.title}_${('000' + innerText.catalogMetadata.catalog.episodeNumber).substr(-3,3)}.json`
        //             await fs.writeFileSync(filename, JSON.stringify(innerText, null, 2));
        //         }
        //     } catch (error) {
        //         console.log(error)
        //     }
        // }
    }


    

    await page.close();

}

async function autoScroll(page){
    await page.evaluate(async () => {
        await new Promise((resolve, reject) => {
            var totalHeight = 0;
            var distance = 100;
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
    } else {
        await page.goto(baseUrl, {waitUntil: 'load', timeout: 0});

        await delay(30000);

        const cookies = await page.cookies();
        await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    let showInfo = [];
    if (fs.existsSync('showInfo.json')) {
        var info = fs.readFileSync('showInfo.json');
        showInfo = JSON.parse(info);
    } else {
        try {
            // await page.goto('https://www.amazon.com/gp/video/storefront/ref=atv_cat_dimension_contenttype_tv_singular?contentType=tv&contentId=home', {waitUntil: 'load', timeout: 15000});
            // await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_13_1?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5IjoiYmJuPTI4NjQ1NDkwMTEmZmllbGQtaXNfcHJpbWVfYmVuZWZpdD0yNDcwOTU1MDExJmZpZWxkLXRoZW1lX2Jyb3dzZS1iaW49MjY1MDM2NTAxMSZzZWFyY2gtYWxpYXM9aW5zdGFudC12aWRlbyZub2RlPTI4NTg3NzgwMTEsMjg2NDU0OTAxMSIsInR4dCI6IktpZHMgYW5kIGZhbWlseSIsIm9mZnNldCI6MCwibnBzaSI6MCwib3JlcSI6ImEwZGI1ODlhLWYxZjMtNGQ2MC1hM2U2LTQ4ZGYxZmU2OTkxNzoxNjQwMzc2NjA5MDAwIiwib3JlcWsiOiJMaTYrby9nc2gwaEdDY1RnYVRnS0x6bWJBenR6bmdvc29lSTA2emFoZmRJPSIsIm9yZXFrdiI6MX0%3D&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
            // await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_13_2?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5Ijoibm9kZT0yODU4Nzc4MDExJnNlYXJjaC1hbGlhcz1pbnN0YW50LXZpZGVvJmZpZWxkLWVudGl0eV90eXBlPTE0MDY5MTg1MDExJmZpZWxkLWlzX3ByaW1lX2JlbmVmaXQ9MjQ3MDk1NTAxMSZmaWVsZC10aGVtZV9icm93c2UtYmluPTI2NTAzNjMwMTEmYmJuPTI4NTg3NzgwMTEiLCJ0eHQiOiJBY3Rpb24gYW5kIGFkdmVudHVyZSIsIm9mZnNldCI6MCwibnBzaSI6MCwib3JlcSI6ImEwZGI1ODlhLWYxZjMtNGQ2MC1hM2U2LTQ4ZGYxZmU2OTkxNzoxNjQwMzc2NjA5MDAwIiwib3JlcWsiOiJMaTYrby9nc2gwaEdDY1RnYVRnS0x6bWJBenR6bmdvc29lSTA2emFoZmRJPSIsIm9yZXFrdiI6MX0%3D&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
            // await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_13_3?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5Ijoibm9kZT0yODU4Nzc4MDExJnNlYXJjaC1hbGlhcz1pbnN0YW50LXZpZGVvJmZpZWxkLWVudGl0eV90eXBlPTE0MDY5MTg1MDExJmZpZWxkLWlzX3ByaW1lX2JlbmVmaXQ9MjQ3MDk1NTAxMSZmaWVsZC10aGVtZV9icm93c2UtYmluPTI2NTAzNjQwMTEmYmJuPTI4NTg3NzgwMTEiLCJ0eHQiOiJBbmltZSIsIm9mZnNldCI6MCwibnBzaSI6MCwib3JlcSI6ImEwZGI1ODlhLWYxZjMtNGQ2MC1hM2U2LTQ4ZGYxZmU2OTkxNzoxNjQwMzc2NjA5MDAwIiwib3JlcWsiOiJMaTYrby9nc2gwaEdDY1RnYVRnS0x6bWJBenR6bmdvc29lSTA2emFoZmRJPSIsIm9yZXFrdiI6MX0%3D&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
            // await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_13_4?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5Ijoibm9kZT0yODU4Nzc4MDExJnNlYXJjaC1hbGlhcz1pbnN0YW50LXZpZGVvJmZpZWxkLWVudGl0eV90eXBlPTE0MDY5MTg1MDExJmZpZWxkLWlzX3ByaW1lX2JlbmVmaXQ9MjQ3MDk1NTAxMSZmaWVsZC10aGVtZV9icm93c2UtYmluPTI2NTAzNjYwMTEmYmJuPTI4NTg3NzgwMTEiLCJ0eHQiOiJDb21lZHkgVFYiLCJvZmZzZXQiOjAsIm5wc2kiOjAsIm9yZXEiOiJhMGRiNTg5YS1mMWYzLTRkNjAtYTNlNi00OGRmMWZlNjk5MTc6MTY0MDM3NjYwOTAwMCIsIm9yZXFrIjoiTGk2K28vZ3NoMGhHQ2NUZ2FUZ0tMem1iQXp0em5nb3NvZUkwNnphaGZkST0iLCJvcmVxa3YiOjF9&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
            // await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_10_5?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5Ijoibm9kZT0yODU4Nzc4MDExJnNlYXJjaC1hbGlhcz1pbnN0YW50LXZpZGVvJmZpZWxkLWVudGl0eV90eXBlPTE0MDY5MTg1MDExJmZpZWxkLWlzX3ByaW1lX2JlbmVmaXQ9MjQ3MDk1NTAxMSZmaWVsZC10aGVtZV9icm93c2UtYmluPTI2NTAzNjcwMTEmYmJuPTI4NTg3NzgwMTEiLCJ0eHQiOiJEb2N1bWVudGFyeSIsIm9mZnNldCI6MCwibnBzaSI6MCwib3JlcSI6ImY1MmYzZTBjLWRlOGQtNDUzMy05MzgzLTI2ZDI5MjU1ZGM5MDoxNjQwNDQ0OTU0MDAwIiwib3JlcWsiOiJMaTYrby9nc2gwaEdDY1RnYVRnS0x6bWJBenR6bmdvc29lSTA2emFoZmRJPSIsIm9yZXFrdiI6MX0%3D&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
            await page.goto('https://www.amazon.com/gp/video/search/ref=atv_tv_hom_c_j40jHe_12_6?queryToken=eyJ0eXBlIjoicXVlcnkiLCJuYXYiOnRydWUsInBpIjoiZGVmYXVsdCIsInNlYyI6ImNlbnRlciIsInN0eXBlIjoic2VhcmNoIiwicXJ5Ijoibm9kZT0yODU4Nzc4MDExJnNlYXJjaC1hbGlhcz1pbnN0YW50LXZpZGVvJmZpZWxkLWVudGl0eV90eXBlPTE0MDY5MTg1MDExJmZpZWxkLWlzX3ByaW1lX2JlbmVmaXQ9MjQ3MDk1NTAxMSZmaWVsZC10aGVtZV9icm93c2UtYmluPTI2NTAzNjgwMTEmYmJuPTI4NTg3NzgwMTEiLCJ0eHQiOiJEcmFtYSBUViIsIm9mZnNldCI6MCwibnBzaSI6MCwib3JlcSI6ImU3YjQ4YTZhLTZmMDQtNDc5Zi04ZWZkLTk4MWQ0YmNlYzJhZjoxNjQwNjEyMzMzMDAwIiwib3JlcWsiOiJMaTYrby9nc2gwaEdDY1RnYVRnS0x6bWJBenR6bmdvc29lSTA2emFoZmRJPSIsIm9yZXFrdiI6MX0%3D&pageId=default&queryPageType=browse&ie=UTF8', {waitUntil: 'load', timeout: 15000});
        } catch (error) {
            console.log(error)
        }
    
        const count = async () => {
            return page.evaluate(() => {
            //    return document.querySelectorAll('.-e5Lt1.tst-card-wrapper.tst-hoverable-card').length;
               return document.querySelectorAll('.av-hover-wrapper').length;
            });
        }
    
        let preCount = 0;
        let postCount = 0;
        do {
            preCount = await count();
            await autoScroll(page);
            await delay(PAGE_SCROLL_DELAY);
            postCount = await count();
            console.log(postCount);
        } while (postCount > preCount);
    
        // const shows = await page.$$('.-e5Lt1.tst-card-wrapper.tst-hoverable-card');
        const shows = await page.$$('.av-hover-wrapper');
        console.log(shows.length)
    
        for (element of shows) {
            var show = await element.$eval('a', node => {return {name:node.getAttribute('aria-label'), url: node.getAttribute('href')}});
            if (show.url.startsWith('/')) {
                show.url = 'https://www.amazon.com' + show.url;
            }
            showInfo.push(show);
        }
        fs.writeFileSync('showInfo.json', JSON.stringify(showInfo));
    }
    

    bluebird.Promise.map(showInfo, (show)=>{
        return new bluebird.Promise(async(resolve) => {
            let page = await browser.newPage();
            await page.setViewport(viewport);
            await getList(browser,show,page).catch((err)=>{console.log(err)});
            resolve();
        });
    },{concurrency:1}).then(()=>{browser.close()}); //Concurrency is set to 10. Lower this value if you have problem loading multiple pages simultaneously or if you have lower internet speeds.
}

run().catch((err)=>{console.log(err)});