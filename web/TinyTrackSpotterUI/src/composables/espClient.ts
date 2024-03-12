import axios from 'axios';

export const useEspClient = (url: string) => ({
  get: async (key: string) => {
    const requestUrl = url + '/settings?' + key;
    const response = await axios.get(requestUrl);
    if(response.status == 200) {
      return  response.data;
    } else {
      throw new Error(response.statusText);
    }
  },
  set: async (key: string, data: string) => {
    const requestUrl = url + '/settings?' + key + '=' + data;
    const response = await axios.post(requestUrl);
    if(response.status == 200) {
      return response.data;
    } else {
      throw new Error(response.statusText);
    }
  }
})
