from server import app
import os

if __name__ == '__main__':
    # Make sure folders exist
    

    
    app.run(host='0.0.0.0', port=5000, debug=True, threaded=True)