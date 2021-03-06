import React, { Component } from "react";
import {
  Platform,
  StyleSheet,
  Text,
  View,
  TouchableOpacity, 
  TimePickerAndroid,
  ActivityIndicator,
  ToastAndroid,
  Picker
} from "react-native";

//Componente para detectar evento de "shake", utilizando el acelerometro.
import RNShakeEvent from 'react-native-shake-event';

//Caracteristica principal de la aplicación. En este componente definimos todas las funciones para
//configurar la alarma y detectar los eventos y estados que esta produzca.
export default class Alarma extends Component {
  constructor(props) {
    super(props);
    this.state = {
      horaAlarma: "22:30", //horario en el que sonara la alarma. configurable desde la app y se almacena en la EEPROM del embebido
      cargandoAlarma: false, //detecta que se esta comunicando con la nube para mostrar gif de cargando
      hora: 22,
      miuntos: 30,
      alarmaActiva: true, //estado de la alarma, indica si va a sonar o no cuando llegue la hora configurada
      sonando: false, //indica si la alarma se encuentra sonando o no para captar y enviar el shake de apagado
      cancion: "0" //numero de cancion configurada, va de 0 a 2
    };

    //Cada 3000ms, consulta al dispositivo si la alarma se encuentra sonando para indicarlo en pantalla
    setInterval(() => {
      this.loadAlarmaSonandoAsync();
    }, 3000);
  }

  //Handler del componente timePicker. Muestra en pantalla el reloj configurable para setear la alarma
  //Al terminar, envia la hora seleccionada al embebido y este la procesa y almacena.
  timePicker = async () => {
    try {
      const { action, hour, minute } = await TimePickerAndroid.open({
        hour: this.state.hora,
        minute: this.state.minutos,
        is24Hour: true, // Will display '2 PM'
        mode: "clock"
      });

      if (action == TimePickerAndroid.timeSetAction) {
        this.setState({ horaAlarma: hour + ':' + ('00'+minute).slice(-2), hora: hour, minutos: minute });
        this.setAlarmaAsync();
      }
    } catch ({ code, message }) {
      console.warn("Error abriendo time picker", message);
    }
  };

  //Evento nativo de react que se ejecuta antes del render, invocamos todas las funciones
  //que obtienen las distintas variables y estados del embebido.
  //Tambien inicializamos el listener para detectar el shake.
  componentWillMount(){
    this.loadCancionAsync();
    this.loadAlarmaAsync();
    this.loadAlarmaActivaAsync();
    RNShakeEvent.addEventListener('shake', () => {
      if (this.state.sonando)
        this.apagarAlarmaAsync();
    });
  }

  render() {
    return (
      <View style={styles.containerGral}>
        <View style={styles.container}>
          <View style={styles.containerAlarma}>
            <ActivityIndicator size="large" color="steelblue" animating={this.state.cargandoAlarma}/>
            {!this.state.cargandoAlarma && 
              <Text style={[styles.textAlarma, !this.state.alarmaActiva && styles.alarmaDesactivada, this.state.sonando && styles.alarmaSonando]}>
                {this.state.horaAlarma}
              </Text> 
            }
          </View>
          <TouchableOpacity style={styles.button} onPress={this.timePicker}>
            <Text>Configurar alarma</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.button} onPress={this.toggleAlarmaAsync}>
            {!this.state.alarmaActiva && <Text>Activar alarma</Text>}
            {this.state.alarmaActiva && <Text>Desactivar alarma</Text>}
          </TouchableOpacity>
        </View>
        <View style={{flex:0.5, alignItems: "center"}}>
          <Text style={{fontSize:20}}>Tonos de alarma</Text>
          <Picker
            selectedValue={this.state.cancion}
            style={styles.ddlCancion}
            onValueChange={(itemValue, itemIndex) => this.changeCancion(itemValue)}>
            <Picker.Item label="Piratas del Caribe" value="0" />
            <Picker.Item label="Yellow Submarine" value="1" />
            <Picker.Item label="Game of Thrones" value="2" />
          </Picker>
        </View>
      </View>
    );
  }

  //Obtiene la cancion configurada en el embebido y la muestra en pantalla.
  loadCancionAsync= async () =>  {
    this.setState({ cargandoAlarma: true});
    fetch('https://api.particle.io/v1/devices/300037000347353137323334/cancion?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'GET',
      headers: {
        Accept: 'application/json'
      }    
    })
    .then((response) => response.json())
    .then((responseJson) => {
      this.setState({ cancion: responseJson.result.toString() });
    })
    .catch((error) => {
      console.error(error);
    });
  }

  //Al seleccionar una cancion, envía al embebido la nueva seleccionada.
  changeCancion = async (cancion) => {

    this.setState({cancion: cancion.toString()});    
    
    fetch('https://api.particle.io/v1/devices/300037000347353137323334/setCancion?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        arg: cancion.toString()
      }) 
    })
    .then((response) => response.json())
    .then((responseJson) => {
      ToastAndroid.show("Tono de alarma configurado correctamente" , ToastAndroid.SHORT);  
    })
    .catch((error) => {
      console.error(error);
    });
  }

  //Envia la hora seleccionada en el picker al embebido.
  setAlarmaAsync() {

    this.setState({ cargandoAlarma: true}); 

    fetch('https://api.particle.io/v1/devices/300037000347353137323334/setAlarma?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        arg: this.state.horaAlarma
      })      
    })
    .then((responseJson) => {
      var returnValue = responseJson.return_value;
      this.setState({cargandoAlarma: false});
      ToastAndroid.show("Alarma configurada correctamente" , ToastAndroid.SHORT);      
    })
    .catch((error) => {
      ToastAndroid.show("Ocurrio un error actualizando la alarma" , ToastAndroid.SHORT);
      console.error(error);
    });
  }

  //Carga la hora configurada para la alarma que esté actualmente en el embebido y la actualiza en pantalla.
  loadAlarmaAsync= async () =>  {
    this.setState({ cargandoAlarma: true});
    fetch('https://api.particle.io/v1/devices/300037000347353137323334/horaalarma?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'GET',
      headers: {
        Accept: 'application/json'
      }    
    })
    .then((response) => response.json())
    .then((responseJson) => {
      var arrAlarma  = responseJson.result.split(":");
      var strAlarma = ('00'+arrAlarma[0]).slice(-2) + ":" + ('00'+arrAlarma[1]).slice(-2);
      this.setState({ horaAlarma: strAlarma, cargandoAlarma: false, hora: parseFloat(arrAlarma[0]), minutos: parseFloat(arrAlarma[1])});
      ToastAndroid.show("Se actualizo la alarma", ToastAndroid.SHORT);

    })
    .catch((error) => {
      console.error(error);
    });
  }

  //Obtiene el estado activo de la alarma para saber si sonará o no cuando llegue la hora y cambia el color del fondo del texto segun su valor
  loadAlarmaActivaAsync= async () =>  {
    fetch('https://api.particle.io/v1/devices/300037000347353137323334/alarmaActiva?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'GET',
      headers: {
        Accept: 'application/json'
      }    
    })
    .then((response) => response.json())
    .then((responseJson) => {
      this.setState({ alarmaActiva: responseJson.result == 1 });
    })
    .catch((error) => {
      console.error(error);
    });
  }

  //Obtiene si la alarma se encuentra sonando y actualiza en pantalla el color del texto segun su valor
  loadAlarmaSonandoAsync= async () =>  {
    if (this.state.sonando) return;

    fetch('https://api.particle.io/v1/devices/300037000347353137323334/sonando?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'GET',
      headers: {
        Accept: 'application/json'
      }    
    })
    .then((response) => response.json())
    .then((responseJson) => {
      this.setState({ sonando: responseJson.result == 1 });
    })
    .catch((error) => {
      console.error(error);
    });
  }

  //Activa o desactiva la alarma y envía este valor al embebido.
  toggleAlarmaAsync = async () =>  {

    this.setState({ cargandoAlarma: true}); 
    var boolActiva = !this.state.alarmaActiva;
    var toggle = boolActiva ? "1" : "0";
    fetch('https://api.particle.io/v1/devices/300037000347353137323334/toggleAlarma?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        arg: toggle
      })      
    })
    .then((responseJson) => {
      this.setState({cargandoAlarma: false, alarmaActiva: boolActiva});
      ToastAndroid.show("Alarma configurada correctamente" , ToastAndroid.SHORT);      
    })
    .catch((error) => {
      ToastAndroid.show("Ocurrio un error actualizando la alarma" , ToastAndroid.SHORT);
      console.error(error);
    });
  }

  //Si la alarma esta sonando y se dtecta un shake, se envia al embebido la informacion de apagado.
  apagarAlarmaAsync = async () =>  {

    this.setState({ cargandoAlarma: true}); 

    fetch('https://api.particle.io/v1/devices/300037000347353137323334/apagarAlarma?access_token=19b2e3af727c4ad7b245755bce7fadb84ac44d74', {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        arg: 0
      })      
    })
    .then((responseJson) => {
      this.setState({cargandoAlarma: false, sonando: false});
      ToastAndroid.show("Alarma detenida correctamente" , ToastAndroid.SHORT);      
    })
    .catch((error) => {
      ToastAndroid.show("Ocurrio un error deteniendo la alarma" , ToastAndroid.SHORT);
      console.error(error);
    });
  }  

  //Al desmontar el componente, dejamos de suscribir al listener del shake
  componentWillUnmount() {
    RNShakeEvent.removeEventListener('shake');
  }

}

//Estilos
const styles = StyleSheet.create({
  containerGral: {
    flex: 2.5,
    borderBottomColor:'grey',
    borderBottomWidth:0.3,
    paddingBottom:25
  },
  container: {
    flex: 2,
    justifyContent: "center",
    paddingHorizontal: 100,
    paddingBottom:10
  },
  button: {
    backgroundColor: "honeydew",
    padding: 5,
    width: 160,
    alignItems: "center",
    marginTop:8,
    marginBottom:8,
    borderColor: "transparent",
    borderWidth: 0,
    borderRadius: 5,  
    
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.8,
    shadowRadius: 2,
    elevation : 1
  },
  containerAlarma: {
    flex:1.5,
    marginBottom:8,
    justifyContent: "center",
    alignItems: "center",
  },
  textAlarma: {
    fontSize: 59,
    textAlign: 'center',
    borderColor: "black",
    borderStyle: "solid",
    borderRadius: 10,
    borderWidth: 1,
    width:160,
  },
  alarmaDesactivada: {
    backgroundColor: "lightgrey"
  },
  alarmaSonando: {
    color: "red"
  },
  ddlCancion:{ 
    height: 50, 
    width: 190,
    borderColor: "black",
    borderStyle: "solid",
    borderRadius: 1,
    borderWidth: 1,  
  }
});
