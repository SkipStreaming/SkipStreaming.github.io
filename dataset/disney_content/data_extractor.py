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

            texts = json_data['data']['DmcVideo']['video']['texts']
            title = ''

            for text in texts:
                if text['field'] == 'title' and text['type'] == 'full' and text['sourceEntity'] == 'series':
                    title = text['content']
                    break
            
            season = json_data['data']['DmcVideo']['video']['seasonSequenceNumber']
            episode = json_data['data']['DmcVideo']['video']['episodeSequenceNumber']

            runtime = json_data['data']['DmcVideo']['video']['mediaMetadata']['runtimeMillis']

            recap_start = None
            recap_end = None
            intro_start = None
            intro_end = None
            up_next = None
            ffer = None
            lfer = None
            ffei = None
            ffec = None

            milestones = json_data['data']['DmcVideo']['video']['milestones']

            for m in milestones:
                if m['milestoneType'] == 'recap_start':
                    recap_start = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'recap_end':
                    recap_end = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'intro_start':
                    intro_start = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'intro_end':
                    intro_end = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'up_next':
                    up_next = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'FFER':
                    ffer = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'LFER':
                    lfer = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'FFEI':
                    ffei = m['milestoneTime'][0]['startMillis']
                elif m['milestoneType'] == 'FFEC':
                    ffec = m['milestoneTime'][0]['startMillis']
            
            sql = '''INSERT INTO disney(title, season, episode, runtime, recap_start, recap_end,
                            intro_start, intro_end, up_next,
                            FFER, LFER, FFEI, FFEC)
                            VALUES ("%s", %d, %d, %d, %s, %s,
                            %s, %s, %s,
                            %s, %s, %s, %s)''' % (title.replace('"', '\\"'), season, episode, runtime, 'NULL' if recap_start is None else str(recap_start), 'NULL' if recap_end is None else str(recap_end),
                            'NULL' if intro_start is None else str(intro_start), 'NULL' if intro_end is None else str(intro_end), 'NULL' if up_next is None else str(up_next),
                            'NULL' if ffer is None else str(ffer), 'NULL' if lfer is None else str(lfer),
                            'NULL' if ffei is None else str(ffei), 'NULL' if ffec is None else str(ffec))
            # print(sql)
                        
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(sql)
                print(e)
                db.rollback()

                
            

