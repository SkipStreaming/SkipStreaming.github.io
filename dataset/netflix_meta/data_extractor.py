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

            type = json_data['video']['type']

            if type == 'show':
                seasons = json_data['video']['seasons']
                # print(seasons)

                for se in seasons:
                    episodes = se['episodes']
                    for eps in episodes:
                        sql = '''INSERT INTO netflix(show_title, issue_year, season, season_id, season_seq, season_short,
                            season_long, episode_seq, episode_id, episode_title, credits_offset, runtime, runtime_display,
                            watched_to_end_offset, credit_start, credit_end, recap_start, recap_end)
                            VALUES ("%s", %d, "%s", "%s", %d, "%s",
                            "%s", %d, "%s", "%s", %d, %d, %d,
                            %d, %d, %d, %d, %d)''' % (json_data['video']['title'].replace('"', '\\"'), se['year'], se['title'].replace('"', '\\"'), str(se['id']).replace('"', '\\"'), se['seq'], se['shortName'].replace('"', '\\"'),
                            se['longName'].replace('"', '\\"'), eps['seq'], str(eps['id']).replace('"', '\\"'), eps['title'].replace('"', '\\"'), eps['creditsOffset'], eps['runtime'], eps['displayRuntime'],
                            eps['watchedToEndOffset'], eps['skipMarkers']['credit']['start'], eps['skipMarkers']['credit']['end'], eps['skipMarkers']['recap']['start'], eps['skipMarkers']['recap']['end'])
                        # print(sql)
                        
                        try:
                            cursor.execute(sql)
                            db.commit()
                        except Exception as e:
                            print(sql)
                            print(e)
                            db.rollback()
            else:
                # print()
                pass

