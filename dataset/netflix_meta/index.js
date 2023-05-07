const puppeteer = require('puppeteer');
const bluebird = require('bluebird');
const sleep = require('delay');
const fs = require('fs')

const CONF = {
	region: 'us',
	username: 'NETFLIX USERNAME',
	password: 'NETFLIX PASSWORD',
	profileLocation: '1',
	pageScrollDelay: 4000,
	pageTimeOut: 45000,
}

//Base URL - DO NOT CHANGE
const baseUrl = "https://www.netflix.com";

const PAGE_SCROLL_DELAY = CONF.pageScrollDelay;

let genreCount = 0;

let showCount = 0;

async function getList(browser, genre, page){
    let url = '';
    if(genre.url.indexOf('?')){
        url = baseUrl+genre.url+'&so=su';
    }
    else{
        url = baseUrl+genre.url+'?so=su';
    }
    
    try {
        await page.goto(url, {waitUntil: 'load', timeout: 15000});
    } catch (error) {
        console.log(error)
    }

    const delay = PAGE_SCROLL_DELAY;
    const wait = (ms) => new Promise(res => setTimeout(res, ms));
    const count = async () => {
        return page.evaluate(() => {
           return document.querySelectorAll('.rowContainer').length;
        });
    }
    const scrollDown = async () => {
        page.evaluate(()=>{
            document.querySelector('.rowContainer:last-child').scrollIntoView({ behavior: 'smooth', block: 'end', inline: 'end' });
        });
    }

    let preCount = 0;
    let postCount = 0;
    do {
        preCount = await count();
        await scrollDown();
        await wait(delay);
        postCount = await count();
    } while (postCount > preCount);
    await wait(delay);

    const titleCards = await page.$$('.title-card-container');

    showCount = showCount + titleCards.length;
    var shows = [];
    
    //Getting all the titles from the page
    for(const card in titleCards){
        if(titleCards.hasOwnProperty(card)){
            const titleCard = titleCards[card];
            let netflixLink = await titleCard.$eval('a', node => node.getAttribute('href'));
            netflixLink = netflixLink.split('?')[0];
            const netflixTitle = await titleCard.$eval('a', node => node.getAttribute('aria-label'));
            const netflixImage = await titleCard.$eval('img', node => node.getAttribute('src'));
            let show = {
                title: netflixTitle,
                image: netflixImage,
                link: netflixLink,
                genre: genre.name,
                type: genre.type
            };
            // console.log(show);
            shows.push(show)
            // await page.goto(`https://www.netflix.com/nq/website/memberapi/vc7a4371d/metadata?movieid=${movie.link.split('/')[2]}`)

            // await sleep(20000);
            //Storing the data to mongo db
        }
    }

    for (let show of shows) {
        let filename = `./data/${show.title.replace(/[^a-z0-9]/gi, '_')}.json`;
        console.log(filename);
        if (fs.existsSync(filename)) {
            continue;
        }
        await page.goto(`https://www.netflix.com/nq/website/memberapi/vc7a4371d/metadata?movieid=${show.link.split('/')[2]}`);
        let innerText = await page.evaluate(() =>  {
            return JSON.parse(document.querySelector("body").innerText); 
        });
        try {
            await fs.writeFileSync(filename, JSON.stringify(innerText, null, 2));
        } catch (error) {
            console.log(error)
        }
        await sleep(1000);
    }
}

async function run() {
    const browser = await puppeteer.launch({
        headless: false, //Set this to false, to enable screen mode and debug
        executablePath: 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe'
    });
    const page = await browser.newPage();
    // dom element selectors
    const usernameSelector = '#id_userLoginId';
    const passwordSelector = '#id_password';
    const loginButtonSelector = 'button.login-button';

    const cookieFile = 'cookies.json';

    if (fs.existsSync(cookieFile)) {
        const cookiesString = await fs.readFileSync(cookieFile);
        const cookies = JSON.parse(cookiesString);
        await page.setCookie(...cookies);
    } else {
        await page.goto(baseUrl+'/'+CONF.region+'/login', {waitUntil: 'load', timeout: 0});

        await page.click(usernameSelector);
        await page.keyboard.type(CONF.username);

        await page.click(passwordSelector);
        await page.keyboard.type(CONF.password);

        await page.click(loginButtonSelector);

        await page.waitForSelector('li.profile');

        await page.click(`li.profile:nth-child(${CONF.profileLocation})`);

        const cookies = await page.cookies();
        await fs.writeFileSync(cookieFile, JSON.stringify(cookies, null, 2));
    }

    let genres = [];

    try {
        await page.goto('https://www.netflix.com/browse/genre/83', {waitUntil: 'load', timeout: 15000});
    } catch (error) {
        console.log(error)
    }

    await page.click('.subgenres');

    await page.waitForSelector('li.sub-menu-item');
    
    const parsedTvGenres = await page.$$('li.sub-menu-item');

    //Get all the tv genres
    for (const key in parsedTvGenres){
        if(parsedTvGenres.hasOwnProperty(key)){
            const element = parsedTvGenres[key];
            const genre = await element.$eval('a', node => {return {name:node.innerText, url: node.getAttribute('href')}});
            genre.type='tvshows';
            genres.push(genre);
        }
    }

    bluebird.Promise.map(genres, (genre)=>{
        return new bluebird.Promise(async(resolve) => {
            await browser.newPage().then(page => getList(browser,genre,page).catch((err)=>{console.log(err)}));
            resolve();
        });
    },{concurrency:10}).then(()=>{browser.close()});
}

run().catch((err)=>{console.log(err)});