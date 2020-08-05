# Calls the Watson Tone Analyzer API (https://www.ibm.com/watson/services/tone-analyzer/)
# to return the tone for a given string of text.

import json
from ibm_watson import ToneAnalyzerV3
from ibm_cloud_sdk_core.authenticators import IAMAuthenticator

# Authenticate user in order to call the API
authenticator = IAMAuthenticator('')
tone_analyzer = ToneAnalyzerV3(
    version='2017-09-21',
    authenticator=authenticator
)

# Set appropriate service URL for API call based on user location
tone_analyzer.set_service_url('')

# Call the API on a given string of text, return sentiments and respective confidence values
def analyze_tone(text):
    tone_analysis = tone_analyzer.tone(
        {'text': text},
        content_type='application/json',
        sentences="false"
    ).get_result()
    # Serialize the resulting JSON
    return json.dumps(tone_analysis)