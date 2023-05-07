import os
import json
import MySQLdb

db = MySQLdb.connect('YOUR DB IP', 'YOUR DB USERNAME', 'YOUR DB PASSWORD', 'YOUR DB NAME', charset='utf8' )
cursor = db.cursor()

for filename in os.listdir('./video_ids'):
    video_ids_filename = os.path.join('./video_ids', filename)
    with open(video_ids_filename,'r',encoding='utf8') as fp:
        json_data = json.load(fp)
        # print (json_data)
        
        filename_no_ext = filename.replace('.json', '')
        title = filename_no_ext.split('_')[0]
        subtitle = filename_no_ext.split('_')[1]
        year = None
        if len(json_data['introduction']['introData']['list']) > 0 and 'year' in json_data['introduction']['introData']['list'][0]['item_params']:
            year = json_data['introduction']['introData']['list'][0]['item_params']['year']
        list_data = json_data['episodeMain']['listData'][0]

        if list_data is None:
            for data in json_data['episodeMain']['listData']:
                if data is not None:
                    list_data = data['list'][0]
                    break
        else:
            list_data = list_data['list'][0]

        for list_element in list_data:
            id_filename = list_element['vid']
            episode = int(list_element['index']) + 1
            with open('./data/%s/%s.json' % (title, id_filename),'r',encoding='utf8') as fp:
                json_data_id = json.load(fp)

            duration = int(json_data_id['results'][0]['fields']['duration'])
            head_time = json_data_id['results'][0]['fields']['head_time']
            tail_time = json_data_id['results'][0]['fields']['tail_time']

            sql = """INSERT INTO tencent(title, episode, subtitle, year, duration, head_time, tail_time)
                VALUES ('%s', %d, '%s', %s, %d, %s, %s)""" % (title, episode, subtitle, 'NULL' if year is None else str(year), duration,
                'NULL' if head_time == 0 else str(head_time), 'NULL' if tail_time == 0 else str(tail_time))
            # print(sql)
            
            try:
                cursor.execute(sql)
                db.commit()
            except Exception as e:
                print(e)
                db.rollback()

