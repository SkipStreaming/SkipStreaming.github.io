import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

for root,dirs,files in os.walk('./data'):
    for file in files:
        filename = os.path.join(root, file)
        with open(filename,'r',encoding='utf8') as fp:
            json_data = json.load(fp)
            # print (json_data)

            ancestors = json_data['catalogMetadata']['family']['tvAncestors']
            title = ''
            season = 0
            for a in ancestors:
                if a['catalog']['type'] == 'SEASON':
                    season = a['catalog']['seasonNumber']
                elif a['catalog']['type'] == 'SHOW':
                    title = a['catalog']['title']
            episode_number = json_data['catalogMetadata']['catalog']['episodeNumber']
            runtime = json_data['catalogMetadata']['catalog']['runtimeSeconds']
            outro_start = json_data['transitionTimecodes']['outroCreditsStart']
            intro_start = None
            intro_end = None
            recap_start = None
            recap_end = None

            if 'skipElements' in json_data['transitionTimecodes'].keys():
                for element in json_data['transitionTimecodes']['skipElements']:
                    if element['elementType'] == 'INTRO':
                        intro_start = element['startTimecodeMs']
                        intro_end = element['endTimecodeMs']
                    elif element['elementType'] == 'RECAP':
                        recap_start = element['startTimecodeMs']
                        recap_end = element['endTimecodeMs']
            
            sql = '''INSERT INTO amazon(title, season_number, episode_number, runtime, outro_start,
                            intro_start, intro_end, recap_start, recap_end)
                            VALUES ("%s", %d, %d, %d, %d,
                            %s, %s, %s, %s)''' % (title.replace('"', '\\"'), season, episode_number, runtime, outro_start,
                            'NULL' if intro_start is None else str(intro_start),
                            'NULL' if intro_end is None else str(intro_end),
                            'NULL' if recap_start is None else str(recap_start),
                            'NULL' if recap_end is None else str(recap_end))
            # print(sql)
                        
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(sql)
                print(e)
                db.rollback()

                
            

