import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

for root,dirs,files in os.walk('./data'):
    for file in files:
        try:
            filename = os.path.join(root, file)

            if filename.endswith('_video.json'):
                continue

            if not os.path.exists(filename.replace('_episode', '_video')):
                continue
                
            fp_episode = open(filename,'r',encoding='utf8')
            fp_video = open(filename.replace('_episode', '_video'),'r',encoding='utf8')

            json_episode = json.load(fp_episode)
            json_video = json.load(fp_video)

            # print (json_data)

            main_credit_start = None

            if 'creditsStartTime' in json_episode[1]['body'].keys():
                main_credit_start = json_episode[1]['body']['creditsStartTime']
            
            title = json_episode[0]['body']['seriesTitles']['full']
            season = None
            episode = None

            if 'seasonNumber' in json_episode[0]['body'].keys() and 'numberInSeason' in json_episode[0]['body'].keys():
                season = json_episode[0]['body']['seasonNumber']
                episode = json_episode[0]['body']['numberInSeason']
            elif 'numberInSeries' in json_episode[0]['body'].keys():
                season = 0
                episode = json_episode[0]['body']['numberInSeries']
            else:
                print('Unable to parse season/episode in %s') % (filename)

            videos = json_video[0]['body']['videos']

            prev_time = 0
            main_duration = None
            duration = None
            main_intro_start = None
            intro_start = None
            main_intro_end = None
            intro_end = None
            credit_start = None

            for video in videos:
                if video['type'] == 'urn:video:main':
                    main_duration = video['duration']

                    credit_start = None if main_credit_start is None else main_credit_start + video['start']

                    if 'annotations' in video.keys():
                        annotations = video['annotations']

                        for a in annotations:
                            if a['secondaryType'] == 'Intro':
                                intro_start = a['start']
                                intro_end = a['end']

                                main_intro_start = intro_start - prev_time
                                main_intro_end = intro_end - prev_time
                            else:
                                print('Unrecognized annotation type "%s" in "%s"' % (a['secondaryType'], filename.replace('_episode', '_video')))
                prev_time = video['start']
                duration = video['start'] + video['duration']

            sql = '''INSERT INTO hbo(title, season, episode, main_duration, main_intro_start, main_intro_end, main_credit_start,
                            duration, intro_start, intro_end, credit_start)
                            VALUES ("%s", %s, %s, %s, %s, %s, %s,
                            %s, %s, %s, %s)''' % (title.replace('"', '\\"'), 'NULL' if season is None else str(season),
                            'NULL' if episode is None else str(episode), 'NULL' if main_duration is None else str(main_duration),
                            'NULL' if main_intro_start is None else str(main_intro_start), 'NULL' if main_intro_end is None else str(main_intro_end),
                            'NULL' if main_credit_start is None else str(main_credit_start), 'NULL' if duration is None else str(duration),
                            'NULL' if intro_start is None else str(intro_start), 'NULL' if intro_end is None else str(intro_end),
                            'NULL' if credit_start is None else str(credit_start))
            # print(sql)
                        
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(sql)
                print(e)
                db.rollback()
        except Exception as e:
            print(e)
                
