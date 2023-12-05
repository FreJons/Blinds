import os
import boto3
import requests

def lambda_handler(event, context):
    # Assuming you have environment variables set for the Timestream database, table, and Discord webhook
    timestream_database = os.environ['Blinds_Database']
    timestream_table = os.environ['Blinds_Data']
    discord_webhook_url = os.environ['DISCORD_WEBHOOK_URL']

    # Create a TimestreamQuery client
    timestream_client = boto3.client('timestream-query')

    # Your logic to query for new open or close events
    query = f"SELECT * FROM {timestream_database}.{timestream_table} WHERE measure_name IN ('Open', 'Closing') ORDER BY time DESC LIMIT 1"

    params = {
        'QueryString': query
    }

    try:
        # Use the TimestreamQuery client to execute the query
        response = timestream_client.query(**params)

        # Extract the relevant information from the query response
        if 'Rows' in response and response['Rows']:
            latest_event = response['Rows'][0]['Data']
            time = latest_event[0]['ScalarValue']
            status = latest_event[1]['ScalarValue']

            # Your logic to format the message
            message = f"New event: {status} at {time}"

            # Your logic to send the message to Discord
            requests.post(discord_webhook_url, json={'content': message})

            return {'statusCode': 200, 'body': 'Notification sent to Discord successfully'}
        else:
            return {'statusCode': 404, 'body': 'No new events found in Timestream'}
    except Exception as e:
        print(f"Error querying Timestream: {e}")
        return {'statusCode': 500, 'body': 'Internal Server Error'}