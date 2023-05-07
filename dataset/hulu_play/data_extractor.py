import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

def str2milli(timestamp: str):
    splts = timestamp.split(':')
    hour = int(splts[0])
    minute = int(splts[1])
    second = int(splts[2].split(';')[0])
    milli = int(splts[2].split(';')[1])
    # print(milli)
    return int((hour * 3600 + minute * 60 + second + 1.0 / 30 * milli) * 1000)

maxx = 0

for root,dirs,files in os.walk('./data'):
    for file in files:
        filename = os.path.join(root, file)
        with open(filename,'r',encoding='utf8') as fp:
            json_data = json.load(fp)
            # print (json_data)

            video_name = json_data['series_name']
            season = int(json_data['season'])
            episode = int(json_data['episode'])

            length = None
            opening_credit_start = None
            opening_credit_end = None
            recap_start = None
            recap_end = None
            end_credit_time = None
            if 'video_metadata' in json_data.keys():
                length = json_data['video_metadata']['length']

                if json_data['video_metadata']['end_credits_time'] is not None and json_data['video_metadata']['end_credits_time'] != '':
                    end_credit_time = str2milli(json_data['video_metadata']['end_credits_time'])
                
                if json_data['video_metadata']['markers'] is not None:
                    for marker in json_data['video_metadata']['markers']:
                        if marker['type'] == 'Opening Credit':
                            opening_credit_start = str2milli(marker['start'])
                            opening_credit_end = str2milli(marker['end'])
                        elif marker['type'] == 'Recap':
                            recap_start = str2milli(marker['start'])
                            recap_end = str2milli(marker['end'])
                        else:
                            print('Unrecognized marker type: %s' % marker['type'])

            sql = '''INSERT INTO hulu(video_name, season, episode, length, opening_credit_start, opening_credit_end,
                            recap_start, recap_end, end_credit_time)
                            VALUES ("%s", %d, %d, %s, %s, %s,
                            %s, %s, %s)''' % (video_name.replace('"', '\\"'), season, episode, 'NULL' if length is None else str(length), 'NULL' if opening_credit_start is None else str(opening_credit_start), 'NULL' if opening_credit_end is None else str(opening_credit_end),
                            'NULL' if recap_start is None else str(recap_start), 'NULL' if recap_end is None else str(recap_end),
                            'NULL' if end_credit_time is None else str(end_credit_time))
            # print(sql)
                        
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(sql)
                print(e)
                db.rollback()

print(maxx)                
            

